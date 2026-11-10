#include "EmergencyProfileSwitcher.h"
#include "PresetCutover.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ro_mesh {

EmergencyProfileSwitcher::EmergencyProfileSwitcher()
  : _enabled(true),
    _channel_set(false),
    _num_tags(0),
    _seen_idx(0),
    _scheduled_at_ts(0),
    _scheduled_preset_id(0),
    _has_scheduled_cutover(false)
{
  memset(_channel_psk, 0, sizeof(_channel_psk));
  memset(_tags, 0, sizeof(_tags));
  memset(_seen_jobs, 0, sizeof(_seen_jobs));
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

bool EmergencyProfileSwitcher::processMessage(const char* message, uint32_t current_ts, CommonRadioPrefs& radio_prefs, RebootFunction reboot_fn) {
  if (!_enabled || !message) return false;

  // First line must start with "!id "
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

  // Check if job was already processed
  if (isJobSeen(job_id)) {
    return false; // Anti-replay
  }
  markJobSeen(job_id);

  bool targets_me = (_num_tags == 0); // If no tags set on node, broadcasts target everyone
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
      PresetCutover::applyPreset(_scheduled_preset_id, radio_prefs);
    }
    if (reboot_fn) {
      reboot_fn();
    }
  }
}

bool EmergencyProfileSwitcher::handleCommand(const char* cmd, char* reply, size_t max_len) {
  if (!cmd || !reply) return false;

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

