#include "PacketFilter.h"
#include "BatteryGate.h"
#include "QuarantineEngine.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace ro_mesh {

PacketFilter& PacketFilter::instance() {
    static PacketFilter inst;
    return inst;
}

PacketFilter::PacketFilter() : _max_hops(7) {
    memset(&_stats, 0, sizeof(_stats));
}

void PacketFilter::init(uint8_t max_hops) {
    _max_hops = max_hops;
    memset(&_stats, 0, sizeof(_stats));
}

void PacketFilter::resetStats() {
    memset(&_stats, 0, sizeof(_stats));
}

FilterVerdict PacketFilter::evaluate(const uint8_t* sender_pubkey,
                                     bool is_advert,
                                     uint8_t hop_count,
                                     uint16_t packet_len,
                                     uint32_t now_ms) {
    _stats.evaluated++;

    // 1. Battery protection gate
    if (!BatteryGate::instance().isRelayAllowed()) {
        _stats.dropped_battery++;
        return FilterVerdict::DROP_BATTERY_LOW;
    }

    // 2. Maximum hop count check
    if (hop_count >= _max_hops) {
        _stats.dropped_hops++;
        return FilterVerdict::DROP_HOP_LIMIT;
    }

    // 3. Quarantine & Blacklist check
    if (sender_pubkey != nullptr && QuarantineEngine::instance().isBanned(sender_pubkey, now_ms)) {
        _stats.dropped_quarantine++;
        return FilterVerdict::DROP_QUARANTINED;
    }

    // 4. Remote Advert 24-hour rate limiting check
    if (is_advert && sender_pubkey != nullptr) {
        bool allow_adv = QuarantineEngine::instance().recordRemoteAdvert(sender_pubkey, now_ms);
        if (!allow_adv) {
            _stats.dropped_rate_limit++;
            return FilterVerdict::DROP_ADVERT_RATE_LIMIT;
        }
    }

    // 5. Airtime token bucket rate limiting check
    if (sender_pubkey != nullptr) {
        uint32_t airtime_ms = QuarantineEngine::calculateAirtimeMs(packet_len);
        bool airtime_ok = QuarantineEngine::instance().recordPacket(sender_pubkey, airtime_ms, now_ms);
        if (!airtime_ok) {
            _stats.dropped_quarantine++;
            return FilterVerdict::DROP_TOKEN_EXHAUSTED;
        }
    }

    _stats.forwarded++;
    return FilterVerdict::ALLOW;
}

bool PacketFilter::handleCliCommand(const char* cmd, char* reply, size_t max_len) {
    if (!cmd || !reply || max_len == 0) return false;

    if (strcmp(cmd, "get filter.stats") == 0) {
        snprintf(reply, max_len,
                 "📊 PacketFilter Stats:\n"
                 "Evaluated: %u | Forwarded: %u\n"
                 "Dropped Battery: %u\n"
                 "Dropped Quarantine: %u\n"
                 "Dropped RateLimit: %u\n"
                 "Dropped Hops: %u",
                 _stats.evaluated, _stats.forwarded,
                 _stats.dropped_battery, _stats.dropped_quarantine,
                 _stats.dropped_rate_limit, _stats.dropped_hops);
        return true;
    }

    if (strncmp(cmd, "set filter.maxhops ", 19) == 0) {
        uint8_t h = (uint8_t)atoi(cmd + 19);
        if (h >= 1 && h <= 15) {
            _max_hops = h;
            snprintf(reply, max_len, "Max hops set to %u", _max_hops);
            return true;
        }
    }

    return false;
}

} // namespace ro_mesh
