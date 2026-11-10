#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

enum class FilterVerdict : uint8_t {
    ALLOW = 0,
    DROP_BATTERY_LOW,
    DROP_QUARANTINED,
    DROP_BLACKLISTED,
    DROP_ADVERT_RATE_LIMIT,
    DROP_TOKEN_EXHAUSTED,
    DROP_HOP_LIMIT
};

struct FilterStats {
    uint32_t evaluated;
    uint32_t forwarded;
    uint32_t dropped_battery;
    uint32_t dropped_quarantine;
    uint32_t dropped_rate_limit;
    uint32_t dropped_hops;
};

class PacketFilter {
public:
    static PacketFilter& instance();

    void init(uint8_t max_hops = 7);

    // Evaluate incoming packet for relay eligibility
    FilterVerdict evaluate(const uint8_t* sender_pubkey,
                           bool is_advert,
                           uint8_t hop_count,
                           uint16_t packet_len,
                           uint32_t now_ms);

    const FilterStats& getStats() const { return _stats; }
    void resetStats();

    void setMaxHops(uint8_t max_hops) { _max_hops = max_hops; }
    uint8_t getMaxHops() const { return _max_hops; }

    bool handleCliCommand(const char* cmd, char* reply, size_t max_len);

private:
    PacketFilter();

    FilterStats _stats;
    uint8_t _max_hops;
};

} // namespace ro_mesh
