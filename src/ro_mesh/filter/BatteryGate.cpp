#include "BatteryGate.h"

namespace ro_mesh {

BatteryGate& BatteryGate::instance() {
    static BatteryGate inst;
    return inst;
}

BatteryGate::BatteryGate()
    : _current_volts(4.10f),
      _cutoff_volts(3.40f),
      _resume_volts(3.60f),
      _relay_allowed(true) {
}

void BatteryGate::init(float cutoff_volts, float resume_volts) {
    _cutoff_volts = cutoff_volts;
    _resume_volts = resume_volts;
    _relay_allowed = true;
}

void BatteryGate::updateVoltage(float volts) {
    _current_volts = volts;

    if (_relay_allowed) {
        if (_current_volts < _cutoff_volts) {
            _relay_allowed = false; // Tripped: disable repeat transmissions to protect battery
        }
    } else {
        if (_current_volts >= _resume_volts) {
            _relay_allowed = true; // Recovered: re-enable relay transmissions
        }
    }
}

} // namespace ro_mesh
