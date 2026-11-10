#include "DeterministicAdverts.h"
#include <string.h>

namespace ro_mesh {

DeterministicAdverts& DeterministicAdverts::instance() {
    static DeterministicAdverts inst;
    return inst;
}

DeterministicAdverts::DeterministicAdverts()
    : _slot_offset_sec(0),
      _next_tx_sec(0),
      _last_tx_sec(0),
      _initialized(false) {
}

uint32_t DeterministicAdverts::computeSlotOffset(const uint8_t* pubkey) {
    if (!pubkey) return 3600; // fallback 1 hour

    // FNV-1a 32-bit hash
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < 32; i++) {
        hash ^= pubkey[i];
        hash *= 16777619u;
    }

    // Deterministic slot within the 23-hour window
    uint32_t offset = hash % CYCLE_DURATION_SEC;
    return offset;
}

void DeterministicAdverts::scheduleNextTx(uint32_t now_sec) {
    uint32_t current_cycle_base = (now_sec / CYCLE_DURATION_SEC) * CYCLE_DURATION_SEC;
    uint32_t candidate = current_cycle_base + _slot_offset_sec;

    // Jitter (+/- 60 seconds) based on low bits of offset
    int32_t jitter = ((int32_t)(_slot_offset_sec & 0x7F)) - 64; // ~[-64, 63]
    if ((int32_t)candidate + jitter > 0) {
        candidate = (uint32_t)((int32_t)candidate + jitter);
    }

    if (candidate <= now_sec) {
        candidate += CYCLE_DURATION_SEC;
    }

    _next_tx_sec = candidate;
}

void DeterministicAdverts::init(const uint8_t* node_pubkey) {
    _slot_offset_sec = computeSlotOffset(node_pubkey);
    _last_tx_sec = 0;
    _initialized = true;
}

bool DeterministicAdverts::shouldTransmitAdvert(uint32_t now_sec) {
    if (!_initialized) return false;

    if (_next_tx_sec == 0) {
        scheduleNextTx(now_sec);
        // On cold boot, don't spam instantly: schedule the first advert after a minimum 60-second warmup
        if (_next_tx_sec - now_sec < 60) {
            _next_tx_sec = now_sec + 60;
        }
    }

    return (now_sec >= _next_tx_sec);
}

void DeterministicAdverts::recordAdvertTransmitted(uint32_t now_sec) {
    _last_tx_sec = now_sec;
    scheduleNextTx(now_sec);
}

void DeterministicAdverts::triggerImmediate() {
    _next_tx_sec = 1; // Forces trigger on next loop
}

} // namespace ro_mesh

