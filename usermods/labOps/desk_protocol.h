#ifndef DESK_PROTOCOL_H
#define DESK_PROTOCOL_H

#include <Arduino.h>
#include "my_config.h"

// ==========================================
// Hardware / Pins
// ==========================================
// Standard ESP32 HardwareSerial 2 Pins
#define DESK_RX_PIN 22 // Connect to Desk TX line
#define DESK_TX_PIN 23 // Connect to Desk RX line
#define DESK_BAUD_RATE 9600

#define BUTTON_KB_PIN 25
#define BUTTON_HS_PIN 27
#define BUTTON_PRESS_DURATION_MS 250

// Optional on-board LED status indicator
#define STATUS_LED_PIN 2 // Default blue LED on DevKit boards

// ==========================================
// Desk Controller Protocol Variant
// ==========================================
// Select your standing desk controller variant:
// - "default" (e.g., Maidesite, Boho Office, Vari Desk - most common)
// - "rocka"   (Rocka specific checksum style)
// - "jarvis"  (Fully Jarvis / Jiecang motor steps style)
#define DESK_VARIANT "default"

// Wake Up Command: If enabled, sends a Stop (wake) command if desk has been
// idle for more than 4 seconds prior to sending movement commands.
#define WAKE_UP_BEFORE_MOVE true
#define IDLE_WAKE_UP_THRESHOLD_SEC 4

// Offsets in millimeters applied when commanding target heights (cm * 10)
#define GO_UP_OFFSET_MM 0
#define GO_DOWN_OFFSET_MM 0

// Default physical height limits (in cm)
#define DEFAULT_MIN_HEIGHT_CM 62.0
#define DEFAULT_MAX_HEIGHT_CM 127.0

class DeskProtocol {
public:
    DeskProtocol();

    // Initialize serial communication and pins
    void init();

    // Process incoming UART bytes from the desk (call this frequently in main loop)
    void process_rx();

    // Increments idle counters, calculates movement states (call this every 1 second in main loop)
    void tick_1s();

    // Actions
    void nudge_up();
    void nudge_down();
    void stop();
    void recall_preset(int id); // id from 1 to 4
    void save_preset(int id);   // id from 1 to 4
    void set_height(double target_cm);

    // Queries to desk
    void query_height();
    void query_limits();

    // Getters for status APIs
    double get_current_height() const { return current_height_cm; }
    double get_min_height() const { return min_height_cm; }
    double get_max_height() const { return max_height_cm; }
    String get_desk_state() const { return desk_state; }
    uint32_t get_idle_seconds() const { return seconds_idle; }
    String get_idle_time_str() const;

    // Preset values decoded from RX packets (in cm)
    double get_preset_height(int id) const;

private:
    // Parsing helper
    void parse_frame(const uint8_t* buffer, size_t len);

    // Wake up utility
    void wake_if_idle();

    // UART TX helper
    void send_command(const uint8_t* cmd, size_t len);
    void send_simple_command(uint8_t cmd_byte);

    // Internal states
    double current_height_cm;
    double last_height_cm;
    double min_height_cm;
    double max_height_cm;
    String desk_state; // "Idle", "Raising", "Lowering"

    uint32_t seconds_idle;
    uint32_t last_activity_ms;

    // Presets decoded from desk feedback (8-byte frames)
    double preset_m1_cm;
    double preset_m2_cm;
    double preset_m3_cm;
    double preset_m4_cm;

    // Jiecang / Fully Jarvis step calculations
    int min_motor_steps;
    int max_motor_steps;
    bool is_calibrating;

    // Circular RX Buffer for stream parsing
    static const size_t RX_BUF_SIZE = 64;
    uint8_t rx_buffer[RX_BUF_SIZE];
    size_t rx_index;
};

// Global instance of the DeskProtocol class
extern DeskProtocol desk;

#endif // DESK_PROTOCOL_H
