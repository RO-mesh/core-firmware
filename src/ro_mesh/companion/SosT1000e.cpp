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
      _is_arming_paused(false),
      _arming_remaining_ms(0),
      _hold_triggered(false),
      _disarm_start_ms(0),
      _last_disarm_beep_sec(0),
      _last_beacon_ms(0),
      _beacons_sent(0),
      _lat(0.0),
      _lon(0.0),
      _altitude(0.0f),
      _has_gps_fix(false),
      _battery_pct(100),
      _current_time_sec(0),
      _send_cb(nullptr),
      _beep_cb(nullptr),
      _sfx_cb(nullptr) {
    
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
    _countdown_start_ms = 0;
    _disarm_start_ms = 0;
    _last_beep_sec = 0;
    _last_disarm_beep_sec = 0;
    _is_arming_paused = false;
    _arming_remaining_ms = 0;
    _hold_triggered = false;
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

void SosT1000e::setCountdown(uint32_t sec) {
    if (sec <= 120) {
        _config.countdown_seconds = sec;
    }
}

void SosT1000e::setContacts(const char* contacts) {
    if (contacts) {
        strncpy(_config.contacts, contacts, sizeof(_config.contacts) - 1);
        _config.contacts[sizeof(_config.contacts) - 1] = '\0';
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

void SosT1000e::updateTime(uint32_t epoch_sec, int8_t tz_offset) {
    if (epoch_sec > 0) {
        _current_time_sec = epoch_sec + ((int32_t)tz_offset * 3600);
    }
}

void SosT1000e::onButtonEvent(bool is_pressed, uint32_t now_ms) {
    if (is_pressed && !_button_last_state) {
        // Button press transition
        _press_start_ms = now_ms;
        _hold_triggered = false;

        // If currently in DISARM countdown, pressing any button immediately aborts deactivation!
        if (_state == SosState::DISARM_COUNTDOWN) {
            _state = SosState::ACTIVE_BEACONING;
            _disarm_start_ms = 0;
            _last_disarm_beep_sec = 0;
            _last_beacon_ms = now_ms; // Reset beacon timer to avoid immediate burst
            if (_sfx_cb) {
                _sfx_cb(SosSoundEffect::DISARM_ABORTED);
            } else if (_beep_cb) {
                _beep_cb(3200, 150);
            }
            _click_count = 0;
            _button_last_state = is_pressed;
            return;
        }

        // If currently in ARMED countdown, freeze/pause countdown immediately on press!
        if (_state == SosState::ARMED_COUNTDOWN && !_is_arming_paused) {
            _is_arming_paused = true;
            uint32_t total_ms = _config.countdown_seconds * 1000;
            uint32_t elapsed_ms = (now_ms > _countdown_start_ms && _countdown_start_ms > 0) ? (now_ms - _countdown_start_ms) : 0;
            _arming_remaining_ms = (total_ms > elapsed_ms) ? (total_ms - elapsed_ms) : 0;
        }
    } else if (!is_pressed && _button_last_state) {
        // Button release transition
        if (_hold_triggered) {
            // Already handled by active hold check in update()
            _hold_triggered = false;
            _click_count = 0;
        } else {
            uint32_t duration = now_ms - _press_start_ms;

            if (duration >= 80 && duration < 800) {
                // Debounced valid short click
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
                    } else if (_state == SosState::ARMED_COUNTDOWN) {
                        cancelSos("Double-click cancel gesture", true);
                        _is_arming_paused = false;
                    } else if (_state == SosState::ACTIVE_BEACONING) {
                        cancelSos("Double-click cancel gesture", false);
                    }
                    _click_count = 0;
                }
            } else if (duration >= 800) {
                // Button released after long non-click tap but before 5000ms hold
                if (_state == SosState::ARMED_COUNTDOWN && _is_arming_paused) {
                    _is_arming_paused = false;
                    uint32_t total_ms = _config.countdown_seconds * 1000;
                    if (total_ms > _arming_remaining_ms) {
                        _countdown_start_ms = now_ms - (total_ms - _arming_remaining_ms);
                        _last_beep_sec = (total_ms - _arming_remaining_ms) / 1000;
                    } else {
                        _countdown_start_ms = now_ms;
                        _last_beep_sec = 0;
                    }
                }
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
    _is_arming_paused = false;
    _arming_remaining_ms = 0;
    _hold_triggered = false;

    if (_beep_cb) {
        _beep_cb(2000, 300); // Initial warning tone
    }
}

void SosT1000e::cancelSos(const char* /*reason*/, bool force_instant) {
    if (_state == SosState::IDLE || _state == SosState::RESOLVED) {
        return;
    }

    _is_arming_paused = false;
    _arming_remaining_ms = 0;

    if (force_instant || _state == SosState::ARMED_COUNTDOWN) {
        bool was_active = (_state == SosState::ACTIVE_BEACONING || _state == SosState::DISARM_COUNTDOWN);
        _state = SosState::RESOLVED;
        _disarm_start_ms = 0;
        _last_disarm_beep_sec = 0;

        if (_sfx_cb) {
            _sfx_cb(SosSoundEffect::COMPLETED_FANFARE);
        } else if (_beep_cb) {
            _beep_cb(1000, 500); // Disarm sound
        }

        if (was_active) {
            transmitResolution();
        }
    } else if (_state == SosState::ACTIVE_BEACONING) {
        // Transition into secure disarm countdown (same interval as arming)
        _state = SosState::DISARM_COUNTDOWN;
        _disarm_start_ms = 0;
        _last_disarm_beep_sec = 0;

        if (_beep_cb) {
            _beep_cb(3000, 150); // Initial descending start tone
        }
    }
}

uint32_t SosT1000e::getRemainingCountdownSec() const {
    if (_state != SosState::ARMED_COUNTDOWN) {
        return 0;
    }
    if (_is_arming_paused) {
        return (_arming_remaining_ms + 999) / 1000;
    }
    if (_countdown_start_ms == 0) {
        return _config.countdown_seconds;
    }
    if (_config.countdown_seconds > _last_beep_sec) {
        return _config.countdown_seconds - _last_beep_sec;
    }
    return 0;
}

uint32_t SosT1000e::getRemainingDisarmSec() const {
    if (_state != SosState::DISARM_COUNTDOWN || _disarm_start_ms == 0) {
        return 0;
    }
    if (_config.countdown_seconds > _last_disarm_beep_sec) {
        return _config.countdown_seconds - _last_disarm_beep_sec;
    }
    return 0;
}

void SosT1000e::update(uint32_t now_ms) {
    // 1. Continuous active hold check (triggers immediately when 5000ms is reached)
    if (_button_last_state && !_hold_triggered) {
        uint32_t hold_duration = now_ms - _press_start_ms;
        if (hold_duration >= 5000) {
            _hold_triggered = true;
            _click_count = 0;
            if (_state == SosState::ARMED_COUNTDOWN) {
                cancelSos("5-second hardware hold gesture", true);
            } else if (_state == SosState::ACTIVE_BEACONING) {
                cancelSos("5-second hardware hold gesture", false);
            }
        }
    }

    // 2. Timeout click count if no subsequent click arrived
    if (_click_count > 0 && (now_ms - _last_click_ms > 700)) {
        _click_count = 0;
        // If arming was paused waiting for potential double-click, and button is released, resume arming
        if (_state == SosState::ARMED_COUNTDOWN && _is_arming_paused && !_button_last_state) {
            _is_arming_paused = false;
            uint32_t total_ms = _config.countdown_seconds * 1000;
            if (total_ms > _arming_remaining_ms) {
                _countdown_start_ms = now_ms - (total_ms - _arming_remaining_ms);
                _last_beep_sec = (total_ms - _arming_remaining_ms) / 1000;
            } else {
                _countdown_start_ms = now_ms;
                _last_beep_sec = 0;
            }
        }
    }

    // 3. State machine progression
    if (_state == SosState::ARMED_COUNTDOWN) {
        if (_countdown_start_ms == 0) {
            _countdown_start_ms = now_ms;
        }
        if (!_is_arming_paused) {
            processCountdown(now_ms);
        }
    } else if (_state == SosState::ACTIVE_BEACONING) {
        uint32_t interval_ms = _config.interval_seconds * 1000;
        if (now_ms - _last_beacon_ms >= interval_ms) {
            transmitSosBeacon(now_ms);
        }
    } else if (_state == SosState::DISARM_COUNTDOWN) {
        if (_disarm_start_ms == 0) {
            _disarm_start_ms = now_ms;
        }
        processDisarmCountdown(now_ms);
    }
}

void SosT1000e::processCountdown(uint32_t now_ms) {
    uint32_t elapsed_ms = now_ms - _countdown_start_ms;
    uint32_t elapsed_sec = elapsed_ms / 1000;

    if (elapsed_sec > _last_beep_sec) {
        _last_beep_sec = elapsed_sec;
        if (_beep_cb && elapsed_sec <= _config.countdown_seconds) {
            // Escalating beeps as arming countdown progresses (1500Hz -> 3000Hz)
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

void SosT1000e::processDisarmCountdown(uint32_t now_ms) {
    uint32_t elapsed_ms = now_ms - _disarm_start_ms;
    uint32_t elapsed_sec = elapsed_ms / 1000;

    if (elapsed_sec > _last_disarm_beep_sec) {
        _last_disarm_beep_sec = elapsed_sec;
        if (_beep_cb && elapsed_sec <= _config.countdown_seconds) {
            // Descending reverse tones (opposite of arming countdown, 3000Hz -> 1400Hz)
            uint32_t freq = 3000;
            if (elapsed_sec * 100 < 1600) {
                freq = 3000 - (elapsed_sec * 100);
            } else {
                freq = 1400;
            }
            _beep_cb(freq, 100);
        }
    }

    if (elapsed_sec >= _config.countdown_seconds) {
        // Countdown reached zero -> deactivation confirmed!
        _state = SosState::RESOLVED;
        _disarm_start_ms = 0;
        _last_disarm_beep_sec = 0;

        if (_sfx_cb) {
            _sfx_cb(SosSoundEffect::COMPLETED_FANFARE);
        } else if (_beep_cb) {
            _beep_cb(1046, 200);
        }

        transmitResolution();
    }
}

void SosT1000e::transmitSosBeacon(uint32_t now_ms) {
    _last_beacon_ms = now_ms;
    _beacons_sent++;

    char time_str[16];
    if (_current_time_sec > 0) {
        uint32_t day_sec = _current_time_sec % 86400;
        uint32_t hour = day_sec / 3600;
        uint32_t min = (day_sec % 3600) / 60;
        snprintf(time_str, sizeof(time_str), "%02u:%02u", hour, min);
    } else {
        strncpy(time_str, "--:--", sizeof(time_str));
    }

    char buffer[256];
    if (_has_gps_fix) {
        snprintf(buffer, sizeof(buffer),
                 "🚨 [SOS] %s | Loc: %.5f,%.5f | Alt: %.0fm | Ora: %s | Batt: %u%% | https://maps.google.com/?q=%.5f,%.5f",
                 _config.custom_message, _lat, _lon, (double)_altitude, time_str, _battery_pct, _lat, _lon);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "🚨 [SOS] %s | Loc: NO-GPS-FIX | Ora: %s | Batt: %u%%",
                 _config.custom_message, time_str, _battery_pct);
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

    if (_sfx_cb) {
        // Radar ping acoustic emulation
        _sfx_cb(SosSoundEffect::RADAR_PING);
    } else if (_beep_cb) {
        _beep_cb(2800, 150);
    }
}

void SosT1000e::transmitResolution() {
    char time_str[16];
    if (_current_time_sec > 0) {
        uint32_t day_sec = _current_time_sec % 86400;
        uint32_t hour = day_sec / 3600;
        uint32_t min = (day_sec % 3600) / 60;
        snprintf(time_str, sizeof(time_str), "%02u:%02u", hour, min);
    } else {
        strncpy(time_str, "--:--", sizeof(time_str));
    }

    char buffer[256];
    if (_has_gps_fix) {
        snprintf(buffer, sizeof(buffer),
                 "✅ [SOS-RESOLVED] Emergency cleared | Loc: %.5f,%.5f | Alt: %.0fm | Ora: %s",
                 _lat, _lon, (double)_altitude, time_str);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "✅ [SOS-RESOLVED] Emergency cleared | Ora: %s",
                 time_str);
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
