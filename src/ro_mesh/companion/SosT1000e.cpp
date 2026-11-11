#include "SosT1000e.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace ro_mesh {

SosT1000e& SosT1000e::instance() {
    static SosT1000e inst;
    return inst;
}

SosT1000e::SosT1000e()
    : _state(SosState::IDLE),
      _button_last_state(false),
      _press_start_ms(0),
      _last_click_ms(0),
      _click_count(0),
      _countdown_start_ms(0),
      _last_beep_sec(0),
      _last_beacon_ms(0),
      _beacons_sent(0),
      _lat(0.0),
      _lon(0.0),
      _altitude(0.0f),
      _has_gps_fix(false),
      _battery_pct(100),
      _send_cb(nullptr),
      _beep_cb(nullptr) {
    
    memset(&_config, 0, sizeof(_config));
    strncpy(_config.callsign, "RO-NODE", sizeof(_config.callsign) - 1);
    strncpy(_config.custom_message, "NEED RESCUE / EMERGENCY ASSISTANCE", sizeof(_config.custom_message) - 1);
    strncpy(_config.channel, "#sos", sizeof(_config.channel) - 1);
    _config.interval_seconds = 60;
    _config.countdown_seconds = 15;
}

void SosT1000e::init() {
    _state = SosState::IDLE;
    _click_count = 0;
    _button_last_state = false;
    _beacons_sent = 0;
}

void SosT1000e::setCallsign(const char* callsign) {
    if (callsign && strlen(callsign) > 0) {
        strncpy(_config.callsign, callsign, sizeof(_config.callsign) - 1);
        _config.callsign[sizeof(_config.callsign) - 1] = '\0';
    }
}

void SosT1000e::setMessage(const char* msg) {
    if (msg && strlen(msg) > 0) {
        strncpy(_config.custom_message, msg, sizeof(_config.custom_message) - 1);
        _config.custom_message[sizeof(_config.custom_message) - 1] = '\0';
    }
}

void SosT1000e::setChannel(const char* ch) {
    if (ch && strlen(ch) > 0) {
        strncpy(_config.channel, ch, sizeof(_config.channel) - 1);
        _config.channel[sizeof(_config.channel) - 1] = '\0';
    }
}

void SosT1000e::setInterval(uint32_t sec) {
    if (sec >= 10 && sec <= 3600) {
        _config.interval_seconds = sec;
    }
}

void SosT1000e::updateGps(double lat, double lon, float altitude, bool has_fix) {
    _lat = lat;
    _lon = lon;
    _altitude = altitude;
    _has_gps_fix = has_fix;
}

void SosT1000e::updateBattery(uint8_t batt_pct) {
    _battery_pct = batt_pct > 100 ? 100 : batt_pct;
}

void SosT1000e::onButtonEvent(bool is_pressed, uint32_t now_ms) {
    if (is_pressed && !_button_last_state) {
        // Button press transition
        _press_start_ms = now_ms;
    } else if (!is_pressed && _button_last_state) {
        // Button release transition
        uint32_t duration = now_ms - _press_start_ms;

        // Long press check (>= 5000ms) cancels / disarms SOS
        if (duration >= 5000) {
            if (_state == SosState::ARMED_COUNTDOWN || _state == SosState::ACTIVE_BEACONING) {
                cancelSos("5-second hardware hold gesture");
            }
            _click_count = 0;
        } else if (duration >= 50 && duration < 800) {
            // Valid short click
            if (now_ms - _last_click_ms < 600) {
                _click_count++;
            } else {
                _click_count = 1;
            }
            _last_click_ms = now_ms;

            // Double click toggles SOS: trigger when Idle/Resolved, cancel when Armed/Active
            if (_click_count == 2) {
                if (_state == SosState::IDLE || _state == SosState::RESOLVED) {
                    triggerManualSos("Double-click gesture");
                } else if (_state == SosState::ARMED_COUNTDOWN || _state == SosState::ACTIVE_BEACONING) {
                    cancelSos("Double-click cancel gesture");
                }
                _click_count = 0;
            }
        }
    }

    _button_last_state = is_pressed;
}

void SosT1000e::triggerManualSos(const char* /*reason*/) {
    _state = SosState::ARMED_COUNTDOWN;
    _countdown_start_ms = 0; // Will be set in update()
    _last_beep_sec = 0;
    _beacons_sent = 0;

    if (_beep_cb) {
        _beep_cb(2000, 300); // Initial warning tone
    }
}

void SosT1000e::cancelSos(const char* reason) {
    bool was_active = (_state == SosState::ACTIVE_BEACONING);
    _state = SosState::RESOLVED;

    if (_beep_cb) {
        _beep_cb(1000, 500); // Disarm sound
    }

    if (was_active) {
        transmitResolution();
    }
    (void)reason;
}

uint32_t SosT1000e::getRemainingCountdownSec() const {
    if (_state != SosState::ARMED_COUNTDOWN || _countdown_start_ms == 0) {
        return 0;
    }
    return _config.countdown_seconds - _last_beep_sec;
}

void SosT1000e::update(uint32_t now_ms) {
    // Timeout click count if no subsequent click arrived
    if (_click_count > 0 && (now_ms - _last_click_ms > 700)) {
        _click_count = 0;
    }

    if (_state == SosState::ARMED_COUNTDOWN) {
        if (_countdown_start_ms == 0) {
            _countdown_start_ms = now_ms;
        }
        processCountdown(now_ms);
    } else if (_state == SosState::ACTIVE_BEACONING) {
        uint32_t interval_ms = _config.interval_seconds * 1000;
        if (now_ms - _last_beacon_ms >= interval_ms) {
            transmitSosBeacon(now_ms);
        }
    }
}

void SosT1000e::processCountdown(uint32_t now_ms) {
    uint32_t elapsed_ms = now_ms - _countdown_start_ms;
    uint32_t elapsed_sec = elapsed_ms / 1000;

    if (elapsed_sec > _last_beep_sec) {
        _last_beep_sec = elapsed_sec;
        if (_beep_cb && elapsed_sec <= _config.countdown_seconds) {
            // Escalating beeps as countdown progresses
            uint32_t freq = 1500 + (elapsed_sec * 100);
            _beep_cb(freq, 100);
        }
    }

    if (elapsed_sec >= _config.countdown_seconds) {
        // Countdown reached zero -> transition to ACTIVE
        _state = SosState::ACTIVE_BEACONING;
        transmitSosBeacon(now_ms);
    }
}

void SosT1000e::transmitSosBeacon(uint32_t now_ms) {
    _last_beacon_ms = now_ms;
    _beacons_sent++;

    char buffer[256];
    if (_has_gps_fix) {
        snprintf(buffer, sizeof(buffer),
                 "[EMERGENCY-SOS] %s: %s | Loc: %.5f,%.5f (alt: %.0fm) | Batt: %u%% | Map: https://maps.google.com/?q=%.5f,%.5f",
                 _config.callsign, _config.custom_message, _lat, _lon, (double)_altitude,
                 _battery_pct, _lat, _lon);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "[EMERGENCY-SOS] %s: %s | Loc: NO-GPS-FIX | Batt: %u%%",
                 _config.callsign, _config.custom_message, _battery_pct);
    }

    // 1. Broadcast to emergency channel (e.g. #sos)
    if (_send_cb) {
        _send_cb(_config.channel, nullptr, buffer);

        // 2. Transmit to direct emergency contacts if configured
        if (strlen(_config.contacts) > 0) {
            char contacts_copy[128];
            strncpy(contacts_copy, _config.contacts, sizeof(contacts_copy) - 1);
            contacts_copy[sizeof(contacts_copy) - 1] = '\0';

            char* token = strtok(contacts_copy, ",");
            while (token != nullptr) {
                // Trim whitespace
                while (*token == ' ') token++;
                if (strlen(token) > 0) {
                    _send_cb(nullptr, token, buffer);
                }
                token = strtok(nullptr, ",");
            }
        }
    }

    if (_beep_cb) {
        // Confirmation burst on TX
        _beep_cb(2800, 200);
    }
}

void SosT1000e::transmitResolution() {
    char buffer[256];
    if (_has_gps_fix) {
        snprintf(buffer, sizeof(buffer),
                 "[SOS-RESOLVED] %s: Emergency cleared | https://maps.google.com/?q=%.5f,%.5f",
                 _config.callsign, _lat, _lon);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "[SOS-RESOLVED] %s: Emergency cleared",
                 _config.callsign);
    }

    if (_send_cb) {
        _send_cb(_config.channel, nullptr, buffer);

        if (strlen(_config.contacts) > 0) {
            char contacts_copy[128];
            strncpy(contacts_copy, _config.contacts, sizeof(contacts_copy) - 1);
            contacts_copy[sizeof(contacts_copy) - 1] = '\0';

            char* token = strtok(contacts_copy, ",");
            while (token != nullptr) {
                while (*token == ' ') token++;
                if (strlen(token) > 0) {
                    _send_cb(nullptr, token, buffer);
                }
                token = strtok(nullptr, ",");
            }
        }
    }
}

} // namespace ro_mesh
