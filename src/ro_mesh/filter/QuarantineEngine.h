#pragma once

#include <Arduino.h>

#define MAX_QUARANTINE_ENTRIES 32
#define MAX_ADVERT_RATE_ENTRIES 64
#define PUB_KEY_LEN 32

namespace ro_mesh {

struct QuarantineEntry {
  uint8_t pub_key[PUB_KEY_LEN];
  uint32_t expires_at_ms;
  uint16_t violations;
  bool active;
};

struct NodeTokenAccount {
  uint8_t pub_key[PUB_KEY_LEN];
  int32_t token_balance_ms;
  uint32_t last_update_ms;
  uint8_t advert_burst_count;
  uint32_t last_advert_window_start_ms;
  bool active;
};

struct AdvertRelayEntry {
  uint8_t pub_key[PUB_KEY_LEN];
  uint32_t last_relayed_ms;
  bool active;
};

class QuarantineEngine {
  bool _enabled;
  uint32_t _max_bucket_ms;         // Default: 5000 ms
  uint32_t _refill_rate_permille;  // Default: 100 (10% duty cycle)
  uint32_t _ban_duration_ms;       // Default: 60 minutes (3,600,000 ms)
  uint8_t _advert_burst_limit;     // Default: 5 adverts per 15 min
  uint32_t _advert_relay_limit_ms; // Default: 24 hours (86,400,000 ms)

  QuarantineEntry _quarantine_table[MAX_QUARANTINE_ENTRIES];
  NodeTokenAccount _token_accounts[MAX_QUARANTINE_ENTRIES];
  AdvertRelayEntry _advert_table[MAX_ADVERT_RATE_ENTRIES];

  QuarantineEngine();

public:
  static QuarantineEngine& instance() {
    static QuarantineEngine inst;
    return inst;
  }

  void setEnabled(bool enabled) { _enabled = enabled; }
  bool isEnabled() const { return _enabled; }

  /**
   * Computes exact LoRa packet airtime in milliseconds using the Semtech formula.
   */
  static uint32_t calculateLoRaAirtimeMs(uint8_t payload_bytes, uint8_t sf = 8, float bw_khz = 62.5f, uint8_t cr = 8);
  static uint32_t calculateAirtimeMs(uint16_t bytes) {
    return calculateLoRaAirtimeMs((uint8_t)(bytes > 255 ? 255 : bytes), 8, 62.5f, 8);
  }

  void init(uint32_t max_bucket_ms = 5000, uint32_t refill_rate_permille = 100);

  /**
   * Checks whether a public key is currently quarantined.
   * If quarantined, returns true (caller should drop packet immediately).
   */
  bool isQuarantined(const uint8_t* pub_key, uint32_t now_ms);
  bool isBanned(const uint8_t* pub_key, uint32_t now_ms) {
    return isQuarantined(pub_key, now_ms);
  }

  /**
   * Checks whether a remote node's advert should be relayed.
   * Enforces universal 24-hour rate limit on all remote node adverts.
   * Returns true if allowed to relay; false if repeat should be dropped.
   */
  bool checkAndRecordAdvertRelay(const uint8_t* pub_key, uint32_t now_ms);
  bool recordRemoteAdvert(const uint8_t* pub_key, uint32_t now_ms) {
    return checkAndRecordAdvertRelay(pub_key, now_ms);
  }

  /**
   * Records packet receipt and updates airtime token bucket for this sender.
   * Automatically quarantines the node if token deficit exceeds limit.
   */
  void recordPacketAirtime(const uint8_t* pub_key, uint32_t airtime_ms, bool is_advert, uint32_t now_ms);
  bool recordPacket(const uint8_t* pub_key, uint32_t airtime_ms, uint32_t now_ms) {
    recordPacketAirtime(pub_key, airtime_ms, false, now_ms);
    return !isQuarantined(pub_key, now_ms);
  }

  // Manual Ban / Unban
  bool addBan(const uint8_t* pub_key, uint32_t duration_minutes, uint32_t now_ms);
  bool removeBan(const uint8_t* pub_key);

  // CLI Command handler: "ban list", "ban add ...", "ban del ..."
  bool handleCommand(const char* cmd, char* reply, size_t max_len, uint32_t now_ms = 0);
};

} // namespace ro_mesh
