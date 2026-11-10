#pragma once

#include <Arduino.h>
#include <helpers/CommonRadioPrefs.h>

namespace ro_mesh {

typedef void (*RebootFunction)();

class EmergencyProfileSwitcher {
  bool _enabled;
  uint8_t _channel_psk[16];
  bool _channel_set;

  char _tags[8][16];
  uint8_t _num_tags;

  char _seen_jobs[32][32];
  uint8_t _seen_idx;

  uint32_t _scheduled_at_ts;
  uint8_t _scheduled_preset_id;
  bool _has_scheduled_cutover;

  EmergencyProfileSwitcher();

public:
  static EmergencyProfileSwitcher& instance() {
    static EmergencyProfileSwitcher inst;
    return inst;
  }

  void setEnabled(bool enabled) { _enabled = enabled; }
  bool isEnabled() const { return _enabled; }

  void setChannelPsk(const uint8_t psk[16]) {
    memcpy(_channel_psk, psk, 16);
    _channel_set = true;
  }

  bool isJobSeen(const char* job_id) const;
  void markJobSeen(const char* job_id);

  bool hasTag(const char* tag) const;
  bool addTag(const char* tag);
  bool removeTag(const char* tag);

  /**
   * Processes incoming text message from the switcher channel.
   * If valid !id and targeted to this node, parses and executes commands.
   */
  bool processMessage(const char* message, uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn);

  /**
   * Periodic check in loop() for scheduled cutovers (!at <ts>).
   */
  void loop(uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn);

  bool handleCommand(const char* cmd, char* reply, size_t max_len);
};

} // namespace ro_mesh

