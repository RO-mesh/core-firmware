#pragma once

#include <Arduino.h>
#include <helpers/CommonRadioPrefs.h>

namespace ro_mesh {

struct RadioPreset {
  uint8_t id;
  const char* name;
  float freq;         // MHz
  float bw;           // kHz
  uint8_t sf;         // 7 to 12
  uint8_t cr;         // 5 to 8 (4/5 to 4/8)
  int8_t target_dbm;  // Target conducted power at antenna
};

// All 3 presets share the official MeshCore EU Narrow baseline frequency (869.618 MHz)
static const RadioPreset OFFICIAL_PRESETS[3] = {
  { 1, "Official MeshCore Narrow", 869.618f, 62.5f, 8, 8, 22 }, // Preset 1
  { 2, "Narrow Balanced SF8 CR5",  869.618f, 62.5f, 8, 5, 22 }, // Preset 2
  { 3, "Narrow Fast SF7 CR5",      869.618f, 62.5f, 7, 5, 22 }  // Preset 3
};

class PresetCutover {
public:
  static const RadioPreset* getPreset(uint8_t id) {
    if (id >= 1 && id <= 3) {
      return &OFFICIAL_PRESETS[id - 1];
    }
    return nullptr;
  }

  /**
   * Applies target preset parameters to radio preferences, factoring in the PA Gain Limiter.
   */
  static bool applyPreset(uint8_t id, CommonRadioPrefs& prefs);
};

} // namespace ro_mesh

