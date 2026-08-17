#include "desk_protocol.h"
#include "wled.h"
#include <ESPAsyncWebServer.h>

void handle_status(AsyncWebServerRequest *request) {
  JsonObject doc = pDoc->to<JsonObject>();
  doc["height"] = desk.get_current_height();
  doc["state"] = desk.get_desk_state();

  JsonObject presets = doc["presets"].to<JsonObject>();
  presets["m1"] = desk.get_preset_height(1);
  presets["m2"] = desk.get_preset_height(2);
  presets["m3"] = desk.get_preset_height(3);
  presets["m4"] = desk.get_preset_height(4);

  JsonObject limits = doc["limits"].to<JsonObject>();
  limits["min"] = desk.get_min_height();
  limits["max"] = desk.get_max_height();

  doc["idleTime"] = desk.get_idle_time_str();

  String response;
  serializeJson(doc, response);
  Serial.println(response);
  request->send(200, "application/json", response);
}

void handle_set_height(AsyncWebServerRequest *request) {
  if (request->hasArg("cm")) {
    double height = request->arg("cm").toDouble();
    Serial.printf("Setting height %.2f cm\n", height);
    desk.set_height(height);
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Missing cm parameter");
  }
}

void handle_command(AsyncWebServerRequest *request) {
  if (request->hasArg("action")) {
    String action = request->arg("action");
    Serial.println("Handling command: " + action);
    if (action == "nudgeup") {
      desk.nudge_up();
    } else if (action == "nudgedown") {
      desk.nudge_down();
    } else if (action == "stop") {
      desk.stop();
    } else if (action == "m1") {
      desk.recall_preset(1);
    } else if (action == "m2") {
      desk.recall_preset(2);
    } else if (action == "m3") {
      desk.recall_preset(3);
    } else if (action == "m4") {
      desk.recall_preset(4);
    } else if (action == "setm1") {
      desk.save_preset(1);
    } else if (action == "setm2") {
      desk.save_preset(2);
    } else if (action == "setm3") {
      desk.save_preset(3);
    } else if (action == "setm4") {
      desk.save_preset(4);
    } else {
      Serial.println("Unknown action: " + action);
      request->send(400, "text/plain", "Unknown action: " + action);
      return;
    }
    request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Missing action parameter");
  }
}

class labOpsUsermod : public Usermod {
private:
  bool initDone = false;
  bool kbPressed = false;
  unsigned long kbPressTime = 0;
  bool hsPressed = false;
  unsigned long hsPressTime = 0;

public:
    void setup() override {
      // Initialization logic if needed
      PinManager::allocatePin(BUTTON_KB_PIN, true, PinOwner::UM_Unspecified);
      PinManager::allocatePin(BUTTON_HS_PIN, true, PinOwner::UM_Unspecified);
      
      // Initialize both pins to LOW (idle state)
      pinMode(BUTTON_KB_PIN, OUTPUT);
      digitalWrite(BUTTON_KB_PIN, LOW);
      
      pinMode(BUTTON_HS_PIN, OUTPUT);
      digitalWrite(BUTTON_HS_PIN, LOW);
      
      desk.init();
      initDone = true;
    }
  void connected() override {
    // Let's print the actual WLED Network IP
    Serial.print("WLED connected! IP: ");
    Serial.println(Network.localIP());

    // WLED uses a bundled version of AsyncJson that requires manual parsing
    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/api/switch/toggle", [this](AsyncWebServerRequest *request) {
          if (request->_tempObject == NULL) {
            request->send(400, "application/json", "{\"error\":\"No body\"}");
            Serial.println("No body");
            return;
          }

          if (!requestJSONBufferLock(JSON_LOCK_SERVER)) {
            request->send(503, "application/json", "{\"error\":\"Busy\"}");
            Serial.println("Busy");
            return;
          }

        // Use WLED's global JSON buffer (pDoc) to save RAM
        // request->_tempObject is not null-terminated, so we must pass its length to deserializeJson
        DeserializationError error = deserializeJson(*pDoc, (const char*)(request->_tempObject), request->contentLength());
        if (error) {
            releaseJSONBufferLock();
            request->send(400, "application/json",
                          "{\"error\":\"Invalid JSON\"}");
            Serial.println("Invalid JSON");
            return;
          }

          JsonObject root = pDoc->as<JsonObject>();
          // Parse explicitly as bool to handle string booleans gracefully
          bool keyboard = root["keyboard"].as<bool>();
          bool headset = root["headset"].as<bool>();
          releaseJSONBufferLock();
          request->send(
              200, "application/json",
              "{\"status\":\"success\",\"keyboard\":" + String(keyboard) +
                  ",\"headset\":" + String(headset) + "}");
          if (keyboard) {
            kbPressed = true;
            kbPressTime = millis();
            // Set to HIGH
            digitalWrite(BUTTON_KB_PIN, HIGH);
            Serial.println("Toggling keyboard ON (HIGH)");
          }
          if (headset) {
            hsPressed = true;
            hsPressTime = millis();
            // Set to HIGH
            digitalWrite(BUTTON_HS_PIN, HIGH);
            Serial.println("Toggling headset ON (HIGH)");
          }
        }));
    // Set up web server routes
    server.on("/api/desk/status", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("Handling desk status request");
      handle_status(request);
    });
    server.on("/api/desk/set_height", HTTP_POST,
              [](AsyncWebServerRequest *request) {
                Serial.println("Handling set desk height request");
                handle_set_height(request);
              });
    server.on("/api/desk/command", HTTP_POST,
              [](AsyncWebServerRequest *request) {
                Serial.println("Handling desk move command request");
                handle_command(request);
              });
  }
  void loop() override {
    // Process incoming desk UART data
    desk.process_rx();
    
    // Handle non-blocking button releases (set back to LOW)
    unsigned long now = millis();
    if (kbPressed && (now - kbPressTime >= BUTTON_PRESS_DURATION_MS)) {
        digitalWrite(BUTTON_KB_PIN, LOW);
        kbPressed = false;
        Serial.println("Toggling keyboard OFF (LOW)");
    }
    if (hsPressed && (now - hsPressTime >= BUTTON_PRESS_DURATION_MS)) {
        digitalWrite(BUTTON_HS_PIN, LOW);
        hsPressed = false;
        Serial.println("Toggling headset OFF (LOW)");
    }
  }

  uint16_t getId() override {
    return 0xABCD; // Use a unique 16-bit ID
  }
};
// Register the usermod so it's automatically included in the build
static labOpsUsermod labOps;
REGISTER_USERMOD(labOps);
