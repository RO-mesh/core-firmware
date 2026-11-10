#pragma once

#include <Arduino.h>

namespace ro_mesh {

enum class TierMode : uint8_t {
  Auto,
  Manual,
  Disabled
};

struct TierDelays {
  float rxdelay;        // seconds
  float txdelay;        // seconds (jitter window)
  float direct_txdelay; // seconds
};

struct TierBucket {
  uint32_t start_time_ms;
  uint16_t packet_count;
  uint16_t duplicate_count;
  float snr_sum;
  uint16_t snr_count;
  uint32_t unique_neighbors[32]; // Hash ring of heard node IDs
  uint8_t neighbor_count;

  void reset(uint32_t now);
  void addNeighbor(uint32_t node_id);
};

class AdaptiveTier {
  TierMode _mode;
  uint8_t _active_tier;      // 0 to 3
  uint8_t _manual_tier;      // 0 to 3
  uint8_t _candidate_tier;
  uint8_t _candidate_confirmations;

  uint32_t _last_eval_ms;
  uint32_t _eval_interval_ms; // default 5 minutes (300,000 ms)

  TierBucket _buckets[3];     // 3 buckets x 5 min = 15 min rolling window
  uint8_t _active_bucket_idx;

  AdaptiveTier();

public:
  static AdaptiveTier& instance() {
    static AdaptiveTier inst;
    return inst;
  }

  void setMode(TierMode mode);
  TierMode getMode() const { return _mode; }

  void setManualTier(uint8_t tier);
  uint8_t getActiveTier() const { return _active_tier; }

  // Returns delay parameters in seconds for active tier
  TierDelays getActiveDelays() const;

  // Called whenever an incoming packet is heard
  void recordPacket(uint32_t sender_id, float snr, bool is_duplicate, uint32_t now_ms);

  // Called regularly in loop()
  void loop(uint32_t now_ms);

  // CLI command handler: "set tier ...", "get tier"
  bool handleCommand(const char* cmd, char* reply, size_t max_len);
};

} // namespace ro_mesh

