#include "TelemetryBeacon.h"
#include "ChannelCli.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace ro_mesh {

TelemetryBeacon& TelemetryBeacon::instance() {
    static TelemetryBeacon inst;
    return inst;
}

TelemetryBeacon::TelemetryBeacon()
    : _current_lat(0.0),
      _current_lon(0.0),
      _current_alt(0.0f),
      _has_gps_fix(false),
      _battery_pct(100),
      _battery_volts(4.12f),
      _last_tx_lat(0.0),
      _last_tx_lon(0.0),
      _last_tx_ms(0),
      _beacons_sent(0),
      _tx_cb(nullptr) {

    _config.enabled = true;
    strncpy(_config.target_channel, "#romania", sizeof(_config.target_channel) - 1);
    _config.target_channel[sizeof(_config.target_channel) - 1] = '\0';
    _config.movement_threshold_m = 100;    // 100 meters
    _config.min_interval_sec = 60;          // 1 minute cooldown
    _config.periodic_interval_sec = 900;   // 15 minutes periodic heartbeat
}

void TelemetryBeacon::init() {
    _last_tx_ms = 0;
    _beacons_sent = 0;
}

bool TelemetryBeacon::setTargetChannel(const char* channel_name) {
    if (!channel_name || strlen(channel_name) == 0) return false;

    // Strict safety bar: Never allow automatic telemetry spam on Public / #public
    if (strcasecmp(channel_name, "Public") == 0 || strcasecmp(channel_name, "#public") == 0) {
        ChannelCli::instance().postSystemNotification("Warning: Telemetry is strictly barred from the Public channel to protect RF airtime. Routed to #romania.");
        strncpy(_config.target_channel, "#romania", sizeof(_config.target_channel) - 1);
        return false;
    }

    strncpy(_config.target_channel, channel_name, sizeof(_config.target_channel) - 1);
    _config.target_channel[sizeof(_config.target_channel) - 1] = '\0';
    return true;
}

void TelemetryBeacon::updateGps(double lat, double lon, float altitude, bool has_fix) {
    _current_lat = lat;
    _current_lon = lon;
    _current_alt = altitude;
    _has_gps_fix = has_fix;
}

void TelemetryBeacon::updateBattery(uint8_t batt_pct, float voltage) {
    _battery_pct = batt_pct > 100 ? 100 : batt_pct;
    _battery_volts = voltage;
}

float TelemetryBeacon::calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // Earth radius in meters
    const double deg2rad = 3.14159265358979323846 / 180.0;

    double dlat = (lat2 - lat1) * deg2rad;
    double dlon = (lon2 - lon1) * deg2rad;

    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(lat1 * deg2rad) * cos(lat2 * deg2rad) *
               sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return (float)(R * c);
}

void TelemetryBeacon::triggerBeacon(uint32_t now_ms, const char* reason) {
    sendBeacon(now_ms, reason);
}

void TelemetryBeacon::sendBeacon(uint32_t now_ms, const char* /*trigger_reason*/) {
    _last_tx_ms = now_ms;
    _last_tx_lat = _current_lat;
    _last_tx_lon = _current_lon;
    _beacons_sent++;

    char buffer[256];
    if (_has_gps_fix) {
        snprintf(buffer, sizeof(buffer),
                 "[TELEMETRY] Loc: %.5f,%.5f (alt: %.0fm) | Batt: %u%% (%.2fV) | Map: https://maps.google.com/?q=%.5f,%.5f",
                 _current_lat, _current_lon, (double)_current_alt,
                 _battery_pct, (double)_battery_volts,
                 _current_lat, _current_lon);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "[TELEMETRY] Loc: NO-GPS-FIX | Batt: %u%% (%.2fV)",
                 _battery_pct, (double)_battery_volts);
    }

    if (_tx_cb) {
        _tx_cb(_config.target_channel, buffer);
    }
}

void TelemetryBeacon::update(uint32_t now_ms) {
    if (!_config.enabled) return;

    uint32_t min_cooldown_ms = _config.min_interval_sec * 1000;
    uint32_t periodic_ms = _config.periodic_interval_sec * 1000;

    // First beacon after boot once GPS fix is acquired
    if (_last_tx_ms == 0 && _has_gps_fix) {
        sendBeacon(now_ms, "initial_boot_fix");
        return;
    }

    // Check if minimum cooldown has elapsed
    if (now_ms - _last_tx_ms >= min_cooldown_ms) {
        // 1. Movement check
        if (_has_gps_fix && _last_tx_lat != 0.0 && _last_tx_lon != 0.0) {
            float dist = calculateDistanceMeters(_last_tx_lat, _last_tx_lon, _current_lat, _current_lon);
            if (dist >= (float)_config.movement_threshold_m) {
                sendBeacon(now_ms, "movement_threshold");
                return;
            }
        }

        // 2. Periodic heartbeat fallback
        if (now_ms - _last_tx_ms >= periodic_ms) {
            sendBeacon(now_ms, "periodic_heartbeat");
            return;
        }
    }
}

} // namespace ro_mesh

