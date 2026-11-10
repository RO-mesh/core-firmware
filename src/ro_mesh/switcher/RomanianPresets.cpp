#include "RomanianPresets.h"
#include "../hal/GainEngine.h"
#include <string.h>
#include <stdio.h>

namespace ro_mesh {

RadioPreset RomanianPresets::s_presets[3];
bool RomanianPresets::s_initialized = false;

void RomanianPresets::initPresets() {
  if (s_initialized) return;

  s_presets[0] = RadioPreset(1, "Official MeshCore Narrow", 869.618f, 62.5f, 8, 5, 22);
  s_presets[1] = RadioPreset(2, "Narrow Balanced SF8 CR5",  869.618f, 62.5f, 8, 5, 22);
  s_presets[2] = RadioPreset(3, "Narrow Fast SF7 CR5",      869.618f, 62.5f, 7, 5, 22);
  s_initialized = true;
}

const RadioPreset* RomanianPresets::getPreset(uint8_t id) {
  initPresets();
  if (id >= 1 && id <= 3) {
    return &s_presets[id - 1];
  }
  return nullptr;
}

bool RomanianPresets::setPreset(uint8_t id, float freq, float bw, uint8_t sf, uint8_t cr, int8_t target_dbm) {
  initPresets();
  if (id < 1 || id > 3) return false;
  if (freq < 150.0f || freq > 960.0f) return false;
  if (bw < 7.8f || bw > 500.0f) return false;
  if (sf < 5 || sf > 12) return false;
  if (cr < 5 || cr > 8) return false;

  RadioPreset& p = s_presets[id - 1];
  p.id = id;
  snprintf(p.name, sizeof(p.name), "Custom Preset %u", id);
  p.freq = freq;
  p.bw = bw;
  p.sf = sf;
  p.cr = cr;
  p.target_dbm = target_dbm;
  return true;
}

bool RomanianPresets::applyPreset(uint8_t id, CommonRadioPrefs& prefs) {
  initPresets();
  const RadioPreset* p = getPreset(id);
  if (!p) return false;

  prefs.setFreq(p->freq);
  prefs.setBandwidth(p->bw);
  prefs.setSpreadFactor(p->sf);
  prefs.setCodingRate(p->cr);

  // Clamp transceiver output using Gain Engine
  int8_t clamped_power = GainEngine::instance().clampTransceiverDrive(p->target_dbm);
  prefs.setTxPower((uint8_t)(clamped_power > 0 ? clamped_power : 0));
  prefs.markDirty();
  return true;
}

} // namespace ro_mesh
