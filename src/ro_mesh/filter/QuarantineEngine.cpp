#include "QuarantineEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ro_mesh {

QuarantineEngine::QuarantineEngine()
  : _enabled(true),
    _max_bucket_ms(5000),
    _refill_rate_permille(100),
    _ban_duration_ms(3600000),      // 60 minutes
    _advert_burst_limit(5),         // 5 adverts per 15 min
    _advert_relay_limit_ms(86400000) // 24 hours
{
  memset(_quarantine_table, 0, sizeof(_quarantine_table));
  memset(_token_accounts, 0, sizeof(_token_accounts));
  memset(_advert_table, 0, sizeof(_advert_table));
}

void QuarantineEngine::init(uint32_t max_bucket_ms, uint32_t refill_rate_permille) {
  _max_bucket_ms = max_bucket_ms;
  _refill_rate_permille = refill_rate_permille;
}

uint32_t QuarantineEngine::calculateLoRaAirtimeMs(uint8_t payload_bytes, uint8_t sf, float bw_khz, uint8_t cr) {
  if (bw_khz <= 0.0f || sf < 6 || sf > 12) return 50;

  float bw_hz = bw_khz * 1000.0f;
  float t_sym_ms = (std::pow(2.0f, sf) / bw_hz) * 1000.0f;

  float n_preamble = 8.0f + 4.25f; // Standard preamble: 8 symbols + 4.25

  bool low_data_rate_opt = (t_sym_ms > 16.0f);
  int de = low_data_rate_opt ? 1 : 0;
  int ih = 0; // Explicit header
  int crc = 1; // CRC enabled
  int cr_denom = (cr >= 5 && cr <= 8) ? (cr - 4) : 1; // 1 to 4 for 4/5..4/8

  int num = 8 * payload_bytes - 4 * sf + 28 + 16 * crc - 20 * ih;
  int den = 4 * (sf - 2 * de);
  if (den <= 0) den = 1;

  float n_payload_raw = std::ceil((float)num / (float)den) * (cr_denom + 4);
  float n_payload = 8.0f + ((n_payload_raw > 0.0f) ? n_payload_raw : 0.0f);

  float total_ms = (n_preamble + n_payload) * t_sym_ms;
  return (uint32_t)std::ceil(total_ms);
}

bool QuarantineEngine::isQuarantined(const uint8_t* pub_key, uint32_t now_ms) {
  if (!_enabled || !pub_key) return false;

  for (int i = 0; i < MAX_QUARANTINE_ENTRIES; i++) {
    if (_quarantine_table[i].active && memcmp(_quarantine_table[i].pub_key, pub_key, PUB_KEY_LEN) == 0) {
      if (now_ms < _quarantine_table[i].expires_at_ms) {
        return true; // Still in quarantine
      } else {
        _quarantine_table[i].active = false; // Expired
      }
    }
  }
  return false;
}

bool QuarantineEngine::checkAndRecordAdvertRelay(const uint8_t* pub_key, uint32_t now_ms) {
  if (!_enabled || !pub_key) return true;

  int free_slot = -1;
  for (int i = 0; i < MAX_ADVERT_RATE_ENTRIES; i++) {
    if (_advert_table[i].active && memcmp(_advert_table[i].pub_key, pub_key, PUB_KEY_LEN) == 0) {
      if (now_ms - _advert_table[i].last_relayed_ms < _advert_relay_limit_ms) {
        return false; // Suppress relay (already forwarded within 24h)
      }
      _advert_table[i].last_relayed_ms = now_ms;
      return true; // 24h passed, permit 1 relay
    }
    if (!_advert_table[i].active && free_slot == -1) {
      free_slot = i;
    }
  }

  // First time seeing this remote node
  int target_slot = (free_slot != -1) ? free_slot : 0;
  memcpy(_advert_table[target_slot].pub_key, pub_key, PUB_KEY_LEN);
  _advert_table[target_slot].last_relayed_ms = now_ms;
  _advert_table[target_slot].active = true;
  return true;
}

void QuarantineEngine::recordPacketAirtime(const uint8_t* pub_key, uint32_t airtime_ms, bool is_advert, uint32_t now_ms) {
  if (!_enabled || !pub_key) return;

  NodeTokenAccount* acc = nullptr;
  int free_slot = -1;

  for (int i = 0; i < MAX_QUARANTINE_ENTRIES; i++) {
    if (_token_accounts[i].active && memcmp(_token_accounts[i].pub_key, pub_key, PUB_KEY_LEN) == 0) {
      acc = &_token_accounts[i];
      break;
    }
    if (!_token_accounts[i].active && free_slot == -1) {
      free_slot = i;
    }
  }

  if (!acc) {
    int slot = (free_slot != -1) ? free_slot : 0;
    acc = &_token_accounts[slot];
    memcpy(acc->pub_key, pub_key, PUB_KEY_LEN);
    acc->token_balance_ms = (int32_t)_max_bucket_ms;
    acc->last_update_ms = now_ms;
    acc->advert_burst_count = 0;
    acc->last_advert_window_start_ms = now_ms;
    acc->active = true;
  }

  // Refill tokens based on elapsed wall time and configured duty cycle
  uint32_t elapsed_ms = now_ms - acc->last_update_ms;
  acc->last_update_ms = now_ms;
  int32_t tokens_to_add = (int32_t)((elapsed_ms * _refill_rate_permille) / 1000);
  acc->token_balance_ms += tokens_to_add;
  if (acc->token_balance_ms > (int32_t)_max_bucket_ms) {
    acc->token_balance_ms = _max_bucket_ms;
  }

  // Deduct packet airtime
  acc->token_balance_ms -= (int32_t)airtime_ms;

  // Check advert burst window (15 minutes)
  if (is_advert) {
    if (now_ms - acc->last_advert_window_start_ms > 900000) {
      acc->last_advert_window_start_ms = now_ms;
      acc->advert_burst_count = 1;
    } else {
      acc->advert_burst_count++;
    }
  }

  // Trip violation if token deficit reaches 0 or below, or excessive advert burst
  if (acc->token_balance_ms < 0 || acc->advert_burst_count > _advert_burst_limit) {
    addBan(pub_key, _ban_duration_ms / 60000, now_ms);
  }
}

bool QuarantineEngine::addBan(const uint8_t* pub_key, uint32_t duration_minutes, uint32_t now_ms) {
  if (!pub_key) return false;

  int free_slot = -1;
  for (int i = 0; i < MAX_QUARANTINE_ENTRIES; i++) {
    if (_quarantine_table[i].active && memcmp(_quarantine_table[i].pub_key, pub_key, PUB_KEY_LEN) == 0) {
      _quarantine_table[i].expires_at_ms = now_ms + (duration_minutes * 60000);
      _quarantine_table[i].violations++;
      return true;
    }
    if (!_quarantine_table[i].active && free_slot == -1) {
      free_slot = i;
    }
  }

  int slot = (free_slot != -1) ? free_slot : 0;
  memcpy(_quarantine_table[slot].pub_key, pub_key, PUB_KEY_LEN);
  _quarantine_table[slot].expires_at_ms = now_ms + (duration_minutes * 60000);
  _quarantine_table[slot].violations = 1;
  _quarantine_table[slot].active = true;
  return true;
}

bool QuarantineEngine::removeBan(const uint8_t* pub_key) {
  if (!pub_key) return false;
  for (int i = 0; i < MAX_QUARANTINE_ENTRIES; i++) {
    if (_quarantine_table[i].active && memcmp(_quarantine_table[i].pub_key, pub_key, PUB_KEY_LEN) == 0) {
      _quarantine_table[i].active = false;
      return true;
    }
  }
  return false;
}

bool QuarantineEngine::handleCommand(const char* cmd, char* reply, size_t max_len, uint32_t now_ms) {
  if (!cmd || !reply) return false;

  if (strcasecmp(cmd, "ban list") == 0) {
    int count = 0;
    size_t offset = 0;
    offset += snprintf(reply + offset, max_len - offset, "Quarantined Nodes:\n");

    for (int i = 0; i < MAX_QUARANTINE_ENTRIES; i++) {
      if (_quarantine_table[i].active && now_ms < _quarantine_table[i].expires_at_ms) {
        count++;
        uint32_t rem_sec = (_quarantine_table[i].expires_at_ms - now_ms) / 1000;
        char hex[9] = {0};
        snprintf(hex, sizeof(hex), "%02X%02X%02X%02X",
                 _quarantine_table[i].pub_key[0], _quarantine_table[i].pub_key[1],
                 _quarantine_table[i].pub_key[2], _quarantine_table[i].pub_key[3]);

        if (offset < max_len - 32) {
          offset += snprintf(reply + offset, max_len - offset, " - Key %s... (rem: %ds)\n", hex, (int)rem_sec);
        }
      }
    }
    if (count == 0) {
      snprintf(reply, max_len, "No active quarantines.");
    }
    return true;
  }

  if (strncasecmp(cmd, "ban add ", 8) == 0) {
    const char* p = cmd + 8;
    while (*p == ' ') p++;
    char hex[16] = {0};
    int i = 0;
    while (*p && *p != ' ' && i < 8) {
      hex[i++] = *p++;
    }
    hex[i] = '\0';
    while (*p == ' ') p++;
    uint32_t dur = (*p) ? (uint32_t)atoi(p) : 60;
    if (dur == 0) dur = 60;

    uint8_t key[PUB_KEY_LEN] = {0};
    for (int k = 0; k < i / 2 && k < 4; k++) {
      char byte_str[3] = { hex[k*2], hex[k*2+1], '\0' };
      key[k] = (uint8_t)strtoul(byte_str, nullptr, 16);
    }

    addBan(key, dur, now_ms);
    snprintf(reply, max_len, "Node %s added to quarantine for %u minutes.", hex, dur);
    return true;
  }

  if (strncasecmp(cmd, "ban del ", 8) == 0) {
    const char* p = cmd + 8;
    while (*p == ' ') p++;
    char hex[16] = {0};
    int i = 0;
    while (*p && *p != ' ' && i < 8) {
      hex[i++] = *p++;
    }
    hex[i] = '\0';

    uint8_t key[PUB_KEY_LEN] = {0};
    for (int k = 0; k < i / 2 && k < 4; k++) {
      char byte_str[3] = { hex[k*2], hex[k*2+1], '\0' };
      key[k] = (uint8_t)strtoul(byte_str, nullptr, 16);
    }

    removeBan(key);
    snprintf(reply, max_len, "Ban for node %s removed.", hex);
    return true;
  }

  return false;
}

} // namespace ro_mesh
