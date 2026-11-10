#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

struct TelemetryConfig {
    bool enabled;
    char target_channel[24];        // Default "#romania"
    uint32_t movement_threshold_m;  // Distance threshold in meters (default 100m)
    uint32_t min_interval_sec;      // Cooldown between movement triggers (default 60s)
    uint32_t periodic_interval_sec; // Periodic heartbeat fallback (default 900s / 15 min)
};

class TelemetryBeacon {
public:
    static TelemetryBeacon& instance();

    void init();
    void update(uint32_t now_ms);

    // Configuration
    TelemetryConfig& getConfig() { return _config; }
    void setEnabled(bool en) { _config.enabled = en; }
    bool isEnabled() const { return _config.enabled; }
    bool setTargetChannel(const char* channel_name);

    // Coordinate & telemetry update inputs
    void updateGps(double lat, double lon, float altitude, bool has_fix);
    void updateBattery(uint8_t batt_pct, float voltage);

    // Manual trigger
    void triggerBeacon(uint32_t now_ms, const char* reason = "manual");

    // Callback for transmitting packet
    typedef void (*TransmitCallback)(const char* target_channel, const char* message);
    void setTransmitCallback(TransmitCallback cb) { _tx_cb = cb; }

private:
    TelemetryBeacon();

    float calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2);
    void sendBeacon(uint32_t now_ms, const char* trigger_reason);

    TelemetryConfig _config;

    double _current_lat;
    double _current_lon;
    float _current_alt;
    bool _has_gps_fix;

    uint8_t _battery_pct;
    float _battery_volts;

    double _last_tx_lat;
    double _last_tx_lon;
    uint32_t _last_tx_ms;
    uint32_t _beacons_sent;

    TransmitCallback _tx_cb;
};

} // namespace ro_mesh

