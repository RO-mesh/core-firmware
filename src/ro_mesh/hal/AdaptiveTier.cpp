#include "AdaptiveTier.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ro_mesh {

static const TierDelays STANDARD_TIER_DELAYS[4] = {
  { 3.0f, 2.0f, 2.0f }, // Tier 0: Backbone / Infrastructure (Peaks, masts)
  { 3.0f, 1.5f, 1.0f }, // Tier 1: Regional / Elevated (Towers, high roofs)
  { 3.0f, 0.8f, 0.4f }, // Tier 2: Local / Suburban (Rooftops)
  { 3.0f, 0.3f, 0.1f }  // Tier 3: Indoor / Personal (Desk nodes, low roof)
};

static const TierDelays DISABLED_TIER_DELAYS = {
  0.0f, 0.2f, 0.0f      // Flat stock MeshCore defaults
};

void TierBucket::reset(uint32_t now) {
  start_time_ms = now;
  packet_count = 0;
  duplicate_count = 0;
  snr_sum = 0.0f;
  snr_count = 0;
  neighbor_count = 0;
  memset(unique_neighbors, 0, sizeof(unique_neighbors));
}

void TierBucket::addNeighbor(uint32_t node_id) {
  if (node_id == 0) return;
  for (uint8_t i = 0; i < neighbor_count; i++) {
    if (unique_neighbors[i] == node_id) return; // Already seen
  }
  if (neighbor_count < 32) {
    unique_neighbors[neighbor_count++] = node_id;
  }
}

AdaptiveTier::AdaptiveTier()
  : _mode(TierMode::Auto),
    _active_tier(1),
    _manual_tier(1),
    _candidate_tier(1),
    _candidate_confirmations(0),
    _last_eval_ms(0),
    _eval_interval_ms(300000), // 5 minutes
    _active_bucket_idx(0)
{
  for (int i = 0; i < 3; i++) {
    _buckets[i].reset(0);
  }
}

void AdaptiveTier::setMode(TierMode mode) {
  _mode = mode;
  if (_mode == TierMode::Manual) {
    _active_tier = _manual_tier;
  }
}

void AdaptiveTier::setManualTier(uint8_t tier) {
  if (tier > 3) tier = 3;
  _manual_tier = tier;
  _mode = TierMode::Manual;
  _active_tier = _manual_tier;
}

TierDelays AdaptiveTier::getActiveDelays() const {
  if (_mode == TierMode::Disabled) {
    return DISABLED_TIER_DELAYS;
  }
  return STANDARD_TIER_DELAYS[_active_tier & 0x03];
}

void AdaptiveTier::recordPacket(uint32_t sender_id, float snr, bool is_duplicate, uint32_t now_ms) {
  TierBucket& b = _buckets[_active_bucket_idx];
  b.packet_count++;
  if (is_duplicate) b.duplicate_count++;
  b.snr_sum += snr;
  b.snr_count++;
  b.addNeighbor(sender_id);
}

void AdaptiveTier::loop(uint32_t now_ms) {
  if (_last_eval_ms == 0) {
    _last_eval_ms = now_ms;
    _buckets[_active_bucket_idx].reset(now_ms);
    return;
  }

  // Check if a 5-minute evaluation tick has arrived
  if (now_ms - _last_eval_ms >= _eval_interval_ms) {
    _last_eval_ms = now_ms;

    // Rotate to next bucket (circular 3-slot ring)
    _active_bucket_idx = (_active_bucket_idx + 1) % 3;
    _buckets[_active_bucket_idx].reset(now_ms);

    if (_mode != TierMode::Auto) return;

    // Aggregate statistics across all 3 buckets (15-minute rolling window)
    uint32_t total_neighbors[96];
    uint8_t total_neighbor_count = 0;
    float total_snr_sum = 0.0f;
    uint32_t total_snr_count = 0;
    uint32_t total_pkts = 0;
    uint32_t total_dups = 0;

    for (int b = 0; b < 3; b++) {
      const TierBucket& bucket = _buckets[b];
      total_pkts += bucket.packet_count;
      total_dups += bucket.duplicate_count;
      total_snr_sum += bucket.snr_sum;
      total_snr_count += bucket.snr_count;

      for (uint8_t n = 0; n < bucket.neighbor_count; n++) {
        uint32_t id = bucket.unique_neighbors[n];
        bool seen = false;
        for (uint8_t k = 0; k < total_neighbor_count; k++) {
          if (total_neighbors[k] == id) { seen = true; break; }
        }
        if (!seen && total_neighbor_count < 96) {
          total_neighbors[total_neighbor_count++] = id;
        }
      }
    }

    float mean_snr = (total_snr_count > 0) ? (total_snr_sum / total_snr_count) : -10.0f;
    float dup_ratio = (total_pkts > 0) ? ((float)total_dups / total_pkts) : 0.0f;

    // Determine target candidate tier
    uint8_t new_target = 2; // Default Local
    if (total_neighbor_count <= 2) {
      new_target = 0; // Isolated / Mountain-top backbone
    } else if (total_neighbor_count <= 7 && mean_snr > -5.0f) {
      new_target = 1; // Regional relay
    } else if (total_neighbor_count >= 10 && dup_ratio > 0.4f) {
      new_target = 3; // Dense urban / indoor node yielding
    } else {
      new_target = 2; // Suburban local
    }

    // Apply 2-cycle hysteresis to prevent flapping
    if (new_target == _candidate_tier) {
      _candidate_confirmations++;
      if (_candidate_confirmations >= 2) {
        _active_tier = _candidate_tier;
      }
    } else {
      _candidate_tier = new_target;
      _candidate_confirmations = 1;
    }
  }
}

bool AdaptiveTier::handleCommand(const char* cmd, char* reply, size_t max_len) {
  if (!cmd || !reply) return false;

  if (strcasecmp(cmd, "get tier") == 0) {
    const char* mode_str = "auto";
    if (_mode == TierMode::Manual) mode_str = "manual";
    if (_mode == TierMode::Disabled) mode_str = "disabled";

    TierDelays d = getActiveDelays();
    snprintf(reply, max_len,
             "tier: %s (active: T%d | rxdelay: %.1fs txdelay: %.1fs direct: %.1fs)",
             mode_str, (int)_active_tier, d.rxdelay, d.txdelay, d.direct_txdelay);
    return true;
  }

  if (strncasecmp(cmd, "set tier ", 9) == 0) {
    const char* val = cmd + 9;
    if (strcasecmp(val, "auto") == 0) {
      setMode(TierMode::Auto);
      snprintf(reply, max_len, "OK - Tier set to AUTO adaptive (active: T%d)", (int)_active_tier);
      return true;
    }
    if (strcasecmp(val, "off") == 0 || strcasecmp(val, "disable") == 0) {
      setMode(TierMode::Disabled);
      snprintf(reply, max_len, "OK - Tier delays DISABLED (Reverted to stock defaults)");
      return true;
    }
    int t = atoi(val);
    if (t >= 0 && t <= 3) {
      setManualTier((uint8_t)t);
      snprintf(reply, max_len, "OK - Tier locked to MANUAL T%d", t);
      return true;
    }
    snprintf(reply, max_len, "Err - Invalid tier (choose auto, off, or 0-3)");
    return true;
  }

  return false;
}

} // namespace ro_mesh
