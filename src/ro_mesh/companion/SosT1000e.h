#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

enum class SosState : uint8_t {
    IDLE = 0,
    ARMED_COUNTDOWN,
    ACTIVE_BEACONING,
    DISARM_COUNTDOWN,
    RESOLVED
};

enum class SosSoundEffect : uint8_t {
    NONE = 0,
    RADAR_PING = 1,
    DISARM_ABORTED = 2,
    COMPLETED_FANFARE = 3
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
    void cancelSos(const char* reason = nullptr, bool force_instant = false);

    // State inspection
    SosState getState() const { return _state; }
    bool isActive() const { return _state == SosState::ACTIVE_BEACONING; }
    bool isArmed() const { return _state == SosState::ARMED_COUNTDOWN; }
    bool isArmingPaused() const { return _is_arming_paused; }
    bool isDisarming() const { return _state == SosState::DISARM_COUNTDOWN; }
    uint32_t getRemainingCountdownSec() const;
    uint32_t getRemainingDisarmSec() const;
    uint32_t getActiveBeaconsSent() const { return _beacons_sent; }

    // Configuration
    SosConfig& getConfig() { return _config; }
    void setCallsign(const char* callsign);
    void setMessage(const char* msg);
    void setChannel(const char* ch);
    void setInterval(uint32_t sec);
    void setCountdown(uint32_t sec);
    void setContacts(const char* contacts);

    // External GPS, Telemetry and Time injection
    void updateGps(double lat, double lon, float altitude, bool has_fix);
    void updateBattery(uint8_t batt_pct);
    void updateTime(uint32_t epoch_sec, int8_t tz_offset = 0);

    // Callback function pointer for sending broadcast / contact message
    typedef void (*SendPacketCallback)(const char* channel, const char* target_contact, const char* message);
    void setSendCallback(SendPacketCallback cb) { _send_cb = cb; }

    // Buzzer / Vibration notification callback
    typedef void (*AlertBeepCallback)(uint32_t freq_hz, uint32_t duration_ms);
    void setBeepCallback(AlertBeepCallback cb) { _beep_cb = cb; }

    typedef void (*AlertSoundEffectCallback)(SosSoundEffect sfx);
    void setSoundEffectCallback(AlertSoundEffectCallback cb) { _sfx_cb = cb; }

private:
    SosT1000e();

    void processCountdown(uint32_t now_ms);
    void processDisarmCountdown(uint32_t now_ms);
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
    bool _is_arming_paused;
    uint32_t _arming_remaining_ms;
    bool _hold_triggered;
    uint32_t _disarm_start_ms;
    uint32_t _last_disarm_beep_sec;
    uint32_t _last_beacon_ms;
    uint32_t _beacons_sent;

    // Telemetry state
    double _lat;
    double _lon;
    float _altitude;
    bool _has_gps_fix;
    uint8_t _battery_pct;
    uint32_t _current_time_sec;

    SendPacketCallback _send_cb;
    AlertBeepCallback _beep_cb;
    AlertSoundEffectCallback _sfx_cb;
};

} // namespace ro_mesh
