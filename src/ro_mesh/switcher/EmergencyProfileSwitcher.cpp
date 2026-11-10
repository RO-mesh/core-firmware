#include "EmergencyProfileSwitcher.h"
#include "PresetCutover.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace ro_mesh {

EmergencyProfileSwitcher::EmergencyProfileSwitcher()
  : _enabled(true),
    _channel_set(false),
    _active_preset_id(1),
    _num_tags(0),
    _seen_idx(0),
    _num_tokens(0),
    _scheduled_at_ts(0),
    _scheduled_preset_id(0),
    _has_scheduled_cutover(false)
{
  memset(_channel_psk, 0, sizeof(_channel_psk));
  strncpy(_sentry_channel, "#ops", sizeof(_sentry_channel) - 1);
  _sentry_channel[sizeof(_sentry_channel) - 1] = '\0';
  memset(_tags, 0, sizeof(_tags));
  memset(_seen_jobs, 0, sizeof(_seen_jobs));
  memset(_tokens, 0, sizeof(_tokens));
}

void EmergencyProfileSwitcher::init() {
  _enabled = true;
  _active_preset_id = 1;
  PresetCutover::initPresets();
}

void EmergencyProfileSwitcher::setSentryChannel(const char* channel_name) {
  if (channel_name && strlen(channel_name) > 0) {
    strncpy(_sentry_channel, channel_name, sizeof(_sentry_channel) - 1);
    _sentry_channel[sizeof(_sentry_channel) - 1] = '\0';
  }
}

void EmergencyProfileSwitcher::clearTokens() {
  memset(_tokens, 0, sizeof(_tokens));
  _num_tokens = 0;
}

bool EmergencyProfileSwitcher::addToken(const char* token) {
  if (!token || strlen(token) == 0 || _num_tokens >= 10) return false;

  // Check duplicate
  for (uint8_t i = 0; i < _num_tokens; i++) {
    if (strcasecmp(_tokens[i].token, token) == 0) return true;
  }

  SentryToken& t = _tokens[_num_tokens++];
  strncpy(t.token, token, sizeof(t.token) - 1);
  t.token[sizeof(t.token) - 1] = '\0';
  t.burned = false;
  return true;
}

bool EmergencyProfileSwitcher::isTokenBurned(const char* token) const {
  if (!token) return true;
  for (uint8_t i = 0; i < _num_tokens; i++) {
    if (strcasecmp(_tokens[i].token, token) == 0) {
      return _tokens[i].burned;
    }
  }
  return true; // Unknown token is treated as invalid/burned
}

bool EmergencyProfileSwitcher::burnToken(const char* token) {
  if (!token) return false;
  for (uint8_t i = 0; i < _num_tokens; i++) {
    if (strcasecmp(_tokens[i].token, token) == 0) {
      if (!_tokens[i].burned) {
        _tokens[i].burned = true;
        return true; // Successfully burned
      }
      return false; // Already burned (replay attempt)
    }
  }
  return false;
}

bool EmergencyProfileSwitcher::isJobSeen(const char* job_id) const {
  if (!job_id || !job_id[0]) return true;
  for (int i = 0; i < 32; i++) {
    if (strcmp(_seen_jobs[i], job_id) == 0) return true;
  }
  return false;
}

void EmergencyProfileSwitcher::markJobSeen(const char* job_id) {
  if (!job_id || !job_id[0]) return;
  strncpy(_seen_jobs[_seen_idx], job_id, sizeof(_seen_jobs[_seen_idx]) - 1);
  _seen_jobs[_seen_idx][sizeof(_seen_jobs[_seen_idx]) - 1] = '\0';
  _seen_idx = (_seen_idx + 1) % 32;
}

bool EmergencyProfileSwitcher::hasTag(const char* tag) const {
  if (!tag) return false;
  for (uint8_t i = 0; i < _num_tags; i++) {
    if (strcasecmp(_tags[i], tag) == 0) return true;
  }
  return false;
}

bool EmergencyProfileSwitcher::addTag(const char* tag) {
  if (!tag || _num_tags >= 8) return false;
  if (hasTag(tag)) return true;
  strncpy(_tags[_num_tags++], tag, 15);
  return true;
}

bool EmergencyProfileSwitcher::removeTag(const char* tag) {
  if (!tag) return false;
  for (uint8_t i = 0; i < _num_tags; i++) {
    if (strcasecmp(_tags[i], tag) == 0) {
      for (uint8_t j = i; j < _num_tags - 1; j++) {
        memcpy(_tags[j], _tags[j + 1], 16);
      }
      _num_tags--;
      return true;
    }
  }
  return false;
}

bool EmergencyProfileSwitcher::parseSteganographicTrigger(const char* message, uint8_t& out_preset, char* out_token, size_t max_token_len) {
  if (!message || !out_token || max_token_len == 0) return false;

  // Format 1: WX Weather Telemetry
  // e.g. "WX: Temp 21.4, Hum 65, Stn# 2-FALCON84"
  const char* stn = strstr(message, "Stn# ");
  if (stn) {
    stn += 5;
    while (*stn == ' ') stn++;
    int p_id = atoi(stn);
    if (p_id >= 1 && p_id <= 3) {
      const char* dash = strchr(stn, '-');
      if (dash) {
        dash++;
        size_t idx = 0;
        while (*dash && !isspace(*dash) && idx < max_token_len - 1) {
          out_token[idx++] = *dash++;
        }
        out_token[idx] = '\0';
        if (strlen(out_token) > 0) {
          out_preset = (uint8_t)p_id;
          return true;
        }
      }
    }
  }

  // Format 2: Routine Check-in
  // e.g. "chk 2 FALCON84 all clear"
  if (strncasecmp(message, "chk ", 4) == 0) {
    const char* p = message + 4;
    while (*p == ' ') p++;
    int p_id = atoi(p);
    if (p_id >= 1 && p_id <= 3) {
      while (*p && !isspace(*p)) p++; // Skip preset number
      while (*p == ' ') p++;          // Skip spaces
      size_t idx = 0;
      while (*p && !isspace(*p) && idx < max_token_len - 1) {
        out_token[idx++] = *p++;
      }
      out_token[idx] = '\0';
      if (strlen(out_token) > 0) {
        out_preset = (uint8_t)p_id;
        return true;
      }
    }
  }

  return false;
}

bool EmergencyProfileSwitcher::processMessage(const char* message, uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn) {
  if (!_enabled || !message) return false;

  // 1. Check for inconspicuous steganographic trigger with single-use token burn
  uint8_t steg_preset = 0;
  char steg_token[16] = {0};
  if (parseSteganographicTrigger(message, steg_preset, steg_token, sizeof(steg_token))) {
    // Attempt single-use token burn
    if (burnToken(steg_token)) {
      _active_preset_id = steg_preset;
      PresetCutover::applyPreset(steg_preset, radio_prefs);
      if (reboot_fn) {
        reboot_fn();
      }
      return true; // Successfully cut over
    } else {
      // Replay attempt or invalid token: silently discard
      return false;
    }
  }

  // 2. Direct job format ("!id ")
  if (strncmp(message, "!id ", 4) != 0) {
    return false;
  }

  char msg_copy[512];
  strncpy(msg_copy, message, sizeof(msg_copy) - 1);
  msg_copy[sizeof(msg_copy) - 1] = '\0';

  char* line = strtok(msg_copy, "\r\n");
  char job_id[32] = {0};
  if (line) {
    strncpy(job_id, line + 4, sizeof(job_id) - 1);
    while (strlen(job_id) > 0 && (job_id[strlen(job_id) - 1] == ' ' || job_id[strlen(job_id) - 1] == '\r')) {
      job_id[strlen(job_id) - 1] = '\0';
    }
  }

  // Check if job was already processed (anti-replay)
  if (isJobSeen(job_id)) {
    return false;
  }
  markJobSeen(job_id);

  bool targets_me = (_num_tags == 0);
  uint32_t at_timestamp = 0;
  uint8_t new_preset = 0;
  bool do_reboot = false;

  while ((line = strtok(nullptr, "\r\n")) != nullptr) {
    while (*line == ' ') line++;

    if (strncmp(line, "!tags ", 6) == 0) {
      char* tag_list = line + 6;
      targets_me = false;
      char* t = strtok(tag_list, ", ");
      while (t) {
        if (hasTag(t)) {
          targets_me = true;
          break;
        }
        t = strtok(nullptr, ", ");
      }
    } else if (strncmp(line, "!at ", 4) == 0) {
      at_timestamp = (uint32_t)strtoul(line + 4, nullptr, 10);
    } else if (strncmp(line, "set preset ", 11) == 0) {
      new_preset = (uint8_t)atoi(line + 11);
    } else if (strcasecmp(line, "reboot") == 0) {
      do_reboot = true;
    }
  }

  if (!targets_me) {
    return false;
  }

  // If scheduled for future
  if (at_timestamp > current_ts) {
    _scheduled_at_ts = at_timestamp;
    _scheduled_preset_id = new_preset;
    _has_scheduled_cutover = true;
    return true;
  }

  // Immediate execution
  if (new_preset >= 1 && new_preset <= 3) {
    _active_preset_id = new_preset;
    PresetCutover::applyPreset(new_preset, radio_prefs);
  }

  if (do_reboot && reboot_fn) {
    reboot_fn();
  }

  return true;
}

void EmergencyProfileSwitcher::loop(uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn) {
  if (_has_scheduled_cutover && current_ts >= _scheduled_at_ts) {
    _has_scheduled_cutover = false;
    if (_scheduled_preset_id >= 1 && _scheduled_preset_id <= 3) {
      _active_preset_id = _scheduled_preset_id;
      PresetCutover::applyPreset(_scheduled_preset_id, radio_prefs);
    }
    if (reboot_fn) {
      reboot_fn();
    }
  }
}

bool EmergencyProfileSwitcher::handleCommand(const char* cmd, char* reply, size_t max_len) {
  if (!cmd || !reply || max_len == 0) return false;

  // 1. sentry status
  if (strcasecmp(cmd, "sentry status") == 0) {
    size_t off = snprintf(reply, max_len,
                          "Sentry: %s | Chan: %s | Active Preset: %u\nTokens (%u/10):\n",
                          _enabled ? "ACTIVE" : "DISABLED", _sentry_channel, _active_preset_id, _num_tokens);
    for (uint8_t i = 0; i < _num_tokens; i++) {
      if (off < max_len - 32) {
        off += snprintf(reply + off, max_len - off, "  [%u] %s (%s)\n",
                        i + 1, _tokens[i].token, _tokens[i].burned ? "BURNED" : "ACTIVE");
      }
    }
    if (_num_tokens == 0 && off < max_len - 20) {
      off += snprintf(reply + off, max_len - off, "  (No tokens configured)\n");
    }
    return true;
  }

  // 2. set sentry.channel <channel>
  if (strncasecmp(cmd, "set sentry.channel ", 19) == 0) {
    const char* ch = cmd + 19;
    while (*ch == ' ') ch++;
    setSentryChannel(ch);
    snprintf(reply, max_len, "OK - Sentry channel set to %s", _sentry_channel);
    return true;
  }

  // 3. set sentry.tokens <t1> <t2> ...
  if (strncasecmp(cmd, "set sentry.tokens ", 18) == 0) {
    clearTokens();
    char buf[128];
    strncpy(buf, cmd + 18, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* t = strtok(buf, " ,");
    while (t != nullptr) {
      addToken(t);
      t = strtok(nullptr, " ,");
    }
    snprintf(reply, max_len, "OK - %u disposable sentry tokens provisioned", _num_tokens);
    return true;
  }

  // 4. set preset <id> <freq> <bw> <sf> <cr>
  if (strncasecmp(cmd, "set preset ", 11) == 0) {
    int id = 0;
    float freq = 0.0f;
    float bw = 0.0f;
    int sf = 0;
    int cr = 0;
    if (sscanf(cmd + 11, "%d %f %f %d %d", &id, &freq, &bw, &sf, &cr) >= 5) {
      if (PresetCutover::setPreset((uint8_t)id, freq, bw, (uint8_t)sf, (uint8_t)cr)) {
        snprintf(reply, max_len, "OK - Preset %d configured (%.3f MHz, %.1f kHz, SF%d, CR4/%d)",
                 id, (double)freq, (double)bw, sf, cr);
        return true;
      }
    }
    snprintf(reply, max_len, "Err - Usage: set preset <id> <freq> <bw> <sf> <cr>");
    return true;
  }

  // 5. Legacy switcher status / tag commands
  if (strcasecmp(cmd, "switcher status") == 0) {
    snprintf(reply, max_len, "Switcher: %s | Chan: %s | Tags: %d | Sched: %s",
             _enabled ? "ON" : "OFF", _channel_set ? "SET" : "NONE",
             (int)_num_tags, _has_scheduled_cutover ? "ARMED" : "NONE");
    return true;
  }

  if (strncasecmp(cmd, "switcher tag add ", 17) == 0) {
    if (addTag(cmd + 17)) {
      snprintf(reply, max_len, "OK - Tag '%s' added", cmd + 17);
    } else {
      snprintf(reply, max_len, "Err - Tag list full or invalid");
    }
    return true;
  }

  return false;
}

} // namespace ro_mesh
