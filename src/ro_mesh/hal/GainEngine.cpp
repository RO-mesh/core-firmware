#include "GainEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ro_mesh {

#ifndef DEFAULT_PA_GAIN_DB
  #if defined(TBEAM_1W) || defined(RAK13300) || defined(HELTEC_T096)
    #define DEFAULT_PA_GAIN_DB 14
  #else
    #define DEFAULT_PA_GAIN_DB 0
  #endif
#endif

GainEngine::GainEngine()
  : _enabled(true),
    _pa_gain_db(DEFAULT_PA_GAIN_DB),
    _legal_limit_dbm(27) // 500 mW ERP default under EU/ANCOM 869.4-869.65 MHz
{
}

int8_t GainEngine::clampTransceiverDrive(int8_t targetConductedDbm) const {
  if (!_enabled || _pa_gain_db <= 0) {
    return targetConductedDbm;
  }

  int8_t max_allowed_drive = _legal_limit_dbm - _pa_gain_db;
  int8_t target_drive = targetConductedDbm - _pa_gain_db;

  if (target_drive > max_allowed_drive) {
    target_drive = max_allowed_drive;
  }
  return target_drive;
}

int8_t GainEngine::getEffectiveConductedPower(int8_t actualTransceiverDriveDbm) const {
  if (_pa_gain_db > 0) {
    return actualTransceiverDriveDbm + _pa_gain_db;
  }
  return actualTransceiverDriveDbm;
}

bool GainEngine::handleCommand(const char* cmd, char* reply, size_t max_len) {
  if (!cmd || !reply) return false;

  if (strcasecmp(cmd, "get pa.status") == 0) {
    snprintf(reply, max_len, "PA Limiter: %s | Registered Gain: %d dB | Legal Cap: %d dBm",
             _enabled ? "ON" : "OFF", (int)_pa_gain_db, (int)_legal_limit_dbm);
    return true;
  }

  if (strncasecmp(cmd, "set pa.limiter ", 15) == 0) {
    const char* val = cmd + 15;
    if (strcasecmp(val, "on") == 0 || strcasecmp(val, "1") == 0) {
      _enabled = true;
      snprintf(reply, max_len, "OK - PA Limiter ENABLED");
      return true;
    } else if (strcasecmp(val, "off") == 0 || strcasecmp(val, "0") == 0) {
      _enabled = false;
      snprintf(reply, max_len, "OK - PA Limiter DISABLED (Full transceiver power unlocked)");
      return true;
    }
  }

  if (strncasecmp(cmd, "set pa.gain ", 12) == 0) {
    int g = atoi(cmd + 12);
    _pa_gain_db = (int8_t)g;
    snprintf(reply, max_len, "OK - PA Gain set to %d dB", g);
    return true;
  }

  return false;
}

} // namespace ro_mesh

