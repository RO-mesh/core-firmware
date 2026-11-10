#pragma once

#include <stdint.h>
#include <stdbool.h>

namespace ro_mesh {

class BatteryGate {
public:
    static BatteryGate& instance();

    void init(float cutoff_volts = 3.40f, float resume_volts = 3.60f);

    void updateVoltage(float volts);

    bool isRelayAllowed() const { return _relay_allowed; }
    float getCurrentVoltage() const { return _current_volts; }
    float getCutoffVoltage() const { return _cutoff_volts; }
    float getResumeVoltage() const { return _resume_volts; }

    void setCutoffVoltage(float cutoff) { _cutoff_volts = cutoff; }
    void setResumeVoltage(float resume) { _resume_volts = resume; }

private:
    BatteryGate();

    float _current_volts;
    float _cutoff_volts;
    float _resume_volts;
    bool _relay_allowed;
};

} // namespace ro_mesh
