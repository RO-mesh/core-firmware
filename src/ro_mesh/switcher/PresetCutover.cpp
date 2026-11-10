#include "PresetCutover.h"
#include "../hal/PaGainLimiter.h"

namespace ro_mesh {

bool PresetCutover::applyPreset(uint8_t id, CommonRadioPrefs& prefs) {
  const RadioPreset* p = getPreset(id);
  if (!p) return false;

  prefs.setFreq(p->freq);
  prefs.setBandwidth(p->bw);
  prefs.setSpreadFactor(p->sf);
  prefs.setCodingRate(p->cr);

  // Compute drive level accounting for PA gain limiter
  int8_t clamped_drive = PaGainLimiter::instance().clampTransceiverDrive(p->target_dbm);
  prefs.setTxPower(clamped_drive);
  prefs.markDirty();

  return true;
}

} // namespace ro_mesh

