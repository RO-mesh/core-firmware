#pragma once

#include <Arduino.h>
#include <helpers/CommonRadioPrefs.h>

namespace ro_mesh {

typedef void (*RebootFunction)();

struct SentryToken {
  char token[16];
  bool burned;
};

class PresetEngine {
  bool _enabled;
  uint8_t _channel_psk[16];
  bool _channel_set;
  char _sentry_channel[24]; // Configurable channel name (e.g. #ops or #alerte)
  uint8_t _active_preset_id;

  char _tags[8][16];
  uint8_t _num_tags;

  char _seen_jobs[32][32];
  uint8_t _seen_idx;

  SentryToken _tokens[10];
  uint8_t _num_tokens;

  uint32_t _scheduled_at_ts;
  uint8_t _scheduled_preset_id;
  bool _has_scheduled_cutover;

  PresetEngine();

public:
  static PresetEngine& instance() {
    static PresetEngine inst;
    return inst;
  }

  void init();

  void setEnabled(bool enabled) { _enabled = enabled; }
  bool isEnabled() const { return _enabled; }

  void setChannelPsk(const uint8_t psk[16]) {
    memcpy(_channel_psk, psk, 16);
    _channel_set = true;
  }

  void setSentryChannel(const char* channel_name);
  const char* getSentryChannel() const { return _sentry_channel; }

  uint8_t getActivePresetId() const { return _active_preset_id; }

  // Disposable Single-Use Token Management ("Burn-After-Reading")
  void clearTokens();
  bool addToken(const char* token);
  bool isTokenBurned(const char* token) const;
  bool burnToken(const char* token);
  uint8_t getTokenCount() const { return _num_tokens; }
  const SentryToken* getToken(uint8_t idx) const {
    if (idx < _num_tokens) return &_tokens[idx];
    return nullptr;
  }

  bool isJobSeen(const char* job_id) const;
  void markJobSeen(const char* job_id);

  bool hasTag(const char* tag) const;
  bool addTag(const char* tag);
  bool removeTag(const char* tag);

  /**
   * Extracts target preset and disposable token from inconspicuous/steganographic message:
   * 1. WX Weather format: "WX: Temp <val>, Hum <val>, Stn# <preset_id>-<token>"
   * 2. Check-in format: "chk <preset_id> <token> [arbitrary text]"
   */
  static bool parseSteganographicTrigger(const char* message, uint8_t& out_preset, char* out_token, size_t max_token_len);

  /**
   * Processes incoming text message from monitored sentry channel.
   * Intercepts both steganographic triggers and !id commands with anti-replay protection.
   */
  bool processMessage(const char* message, uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn = nullptr);

  /**
   * Periodic check in loop() for scheduled cutovers (!at <ts>).
   */
  void loop(uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn = nullptr);
  void update(uint32_t now_ms) {
    // Convenience alias for main loop
    (void)now_ms;
  }

  bool handleCommand(const char* cmd, char* reply, size_t max_len);
};

} // namespace ro_mesh
