#pragma once

#include <Arduino.h>
#include <helpers/CommonRadioPrefs.h>

namespace ro_mesh {

struct RadioPreset {
  uint8_t id;
  char name[32];
  float freq;         // MHz
  float bw;           // kHz
  uint8_t sf;         // 7 to 12
  uint8_t cr;         // 5 to 8 (4/5 to 4/8)
  int8_t target_dbm;  // Target conducted power at antenna
};

class PresetCutover {
  static RadioPreset s_presets[3];
  static bool s_initialized;

public:
  static void initPresets();
  static const RadioPreset* getPreset(uint8_t id);
  static bool setPreset(uint8_t id, float freq, float bw, uint8_t sf, uint8_t cr, int8_t target_dbm = 22);

  /**
   * Applies target preset parameters to radio preferences, factoring in the PA Gain Limiter.
   */
  static bool applyPreset(uint8_t id, CommonRadioPrefs& prefs);
};

} // namespace ro_mesh
