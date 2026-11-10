#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

enum class SosState : uint8_t {
    IDLE = 0,
    ARMED_COUNTDOWN,
    ACTIVE_BEACONING,
    RESOLVED
};

struct SosConfig {
    char callsign[16];
    char custom_message[64];
    char channel[24];            // Default "#sos"
    char contacts[128];          // Comma-separated public keys or names
    uint32_t interval_seconds;   // Default 60 seconds
    uint32_t countdown_seconds;  // Default 15 seconds
};

class SosT1000e {
public:
    static SosT1000e& instance();

    void init();
    void update(uint32_t now_ms);

    // Hardware button event inputs
    void onButtonEvent(bool is_pressed, uint32_t now_ms);
    void triggerManualSos(const char* reason = nullptr);
    void cancelSos(const char* reason = nullptr);

    // State inspection
    SosState getState() const { return _state; }
    bool isActive() const { return _state == SosState::ACTIVE_BEACONING; }
    bool isArmed() const { return _state == SosState::ARMED_COUNTDOWN; }
    uint32_t getRemainingCountdownSec() const;
    uint32_t getActiveBeaconsSent() const { return _beacons_sent; }

    // Configuration
    SosConfig& getConfig() { return _config; }
    void setCallsign(const char* callsign);
    void setMessage(const char* msg);
    void setChannel(const char* ch);
    void setInterval(uint32_t sec);

    // External GPS and Telemetry injection
    void updateGps(double lat, double lon, float altitude, bool has_fix);
    void updateBattery(uint8_t batt_pct);

    // Callback function pointer for sending broadcast / contact message
    typedef void (*SendPacketCallback)(const char* channel, const char* target_contact, const char* message);
    void setSendCallback(SendPacketCallback cb) { _send_cb = cb; }

    // Buzzer / Vibration notification callback
    typedef void (*AlertBeepCallback)(uint32_t freq_hz, uint32_t duration_ms);
    void setBeepCallback(AlertBeepCallback cb) { _beep_cb = cb; }

private:
    SosT1000e();

    void processCountdown(uint32_t now_ms);
    void transmitSosBeacon(uint32_t now_ms);
    void transmitResolution();

    SosState _state;
    SosConfig _config;

    // Hardware Button Tracking
    bool _button_last_state;
    uint32_t _press_start_ms;
    uint32_t _last_click_ms;
    uint8_t _click_count;

    // Countdown and beacon timing
    uint32_t _countdown_start_ms;
    uint32_t _last_beep_sec;
    uint32_t _last_beacon_ms;
    uint32_t _beacons_sent;

    // Telemetry state
    double _lat;
    double _lon;
    float _altitude;
    bool _has_gps_fix;
    uint8_t _battery_pct;

    SendPacketCallback _send_cb;
    AlertBeepCallback _beep_cb;
};

} // namespace ro_mesh
