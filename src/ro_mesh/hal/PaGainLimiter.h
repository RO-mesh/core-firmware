#pragma once

#include <Arduino.h>

namespace ro_mesh {

class PaGainLimiter {
  bool _enabled;
  int8_t _pa_gain_db;
  int8_t _legal_limit_dbm;

  PaGainLimiter();

public:
  static PaGainLimiter& instance() {
    static PaGainLimiter inst;
    return inst;
  }

  void setLimiterEnabled(bool enabled) { _enabled = enabled; }
  bool isLimiterEnabled() const { return _enabled; }

  void setPaGain(int8_t gainDb) { _pa_gain_db = gainDb; }
  int8_t getPaGain() const { return _pa_gain_db; }

  void setLegalLimit(int8_t limitDbm) { _legal_limit_dbm = limitDbm; }
  int8_t getLegalLimit() const { return _legal_limit_dbm; }

  /**
   * Computes the required transceiver drive level (dBm) for a given target conducted power.
   * If a +14 dB PA is present and target is +22 dBm, returns +8 dBm drive level.
   */
  int8_t clampTransceiverDrive(int8_t targetConductedDbm) const;

  /**
   * Computes the actual conducted output at the antenna connector.
   */
  int8_t getEffectiveConductedPower(int8_t actualTransceiverDriveDbm) const;

  /**
   * Processes CLI commands:
   *   "set pa.limiter on|off"
   *   "set pa.gain <dB>"
   *   "get pa.status"
   */
  bool handleCommand(const char* cmd, char* reply, size_t max_len);
};

} // namespace ro_mesh

