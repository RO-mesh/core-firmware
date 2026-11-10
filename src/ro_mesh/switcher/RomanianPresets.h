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

  RadioPreset() : id(0), freq(0.0f), bw(0.0f), sf(0), cr(0), target_dbm(0) {
    name[0] = '\0';
  }

  RadioPreset(uint8_t _id, const char* _name, float _f, float _bw, uint8_t _sf, uint8_t _cr, int8_t _pwr)
    : id(_id), freq(_f), bw(_bw), sf(_sf), cr(_cr), target_dbm(_pwr) {
    strncpy(name, _name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
  }
};

class RomanianPresets {
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
