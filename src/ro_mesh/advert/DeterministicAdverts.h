#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

class DeterministicAdverts {
public:
    static constexpr uint32_t CYCLE_DURATION_SEC = 23 * 3600; // 23-hour rolling window

    static DeterministicAdverts& instance();

    void init(const uint8_t* node_pubkey);

    // Call in main loop to check if it's time to transmit self-originated advert
    bool shouldTransmitAdvert(uint32_t now_sec);

    // Record that advert was transmitted
    void recordAdvertTransmitted(uint32_t now_sec);

    // Configuration / status
    uint32_t getSlotOffsetSec() const { return _slot_offset_sec; }
    uint32_t getNextTxSec() const { return _next_tx_sec; }
    uint32_t getCycleDurationSec() const { return CYCLE_DURATION_SEC; }

    // Manual immediate trigger override
    void triggerImmediate();

private:
    DeterministicAdverts();

    uint32_t computeSlotOffset(const uint8_t* pubkey);
    void scheduleNextTx(uint32_t now_sec);

    uint32_t _slot_offset_sec;
    uint32_t _next_tx_sec;
    uint32_t _last_tx_sec;
    bool _initialized;
};

} // namespace ro_mesh

