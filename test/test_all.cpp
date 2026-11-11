#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/ro_mesh/regions/RomanianRegions.h"
#include "../src/ro_mesh/regions/AutomaticRegions.h"
#include "../src/ro_mesh/hal/AutomaticTiers.h"
#include "../ro_mesh/hal/GainEngine.h"
#include "../src/ro_mesh/filter/QuarantineEngine.h"
#include "../src/ro_mesh/filter/BatteryGate.h"
#include "../src/ro_mesh/filter/FilterEngine.h"
#include "../src/ro_mesh/switcher/RomanianPresets.h"
#include "../src/ro_mesh/switcher/PresetEngine.h"
#include "../src/ro_mesh/advert/SmartAdverts.h"
#include "../src/ro_mesh/companion/SosT1000e.h"
#include "../src/ro_mesh/companion/CliAssistant.h"
#include "../src/ro_mesh/companion/TelemetryBeacon.h"
#include "../src/ro_mesh/companion/CliContact.h"
#include "../src/ro_mesh/companion/RomanianChannels.h"
#include "../src/ro_mesh/companion/MessageStorage.h"

MockSerial Serial;

using namespace ro_mesh;

// Global test event capture
static char g_last_system_msg[512] = {0};
static char g_last_sos_msg[512] = {0};
static char g_last_sos_channel[64] = {0};
static char g_last_telemetry_msg[512] = {0};
static char g_accumulated_system_msg[2048] = {0};
static int g_system_msg_count = 0;

static void testInjectSystemMessage(const uint8_t* /*target_id*/, bool /*is_channel*/, const char* message) {
    strncpy(g_last_system_msg, message, sizeof(g_last_system_msg) - 1);
    strncat(g_accumulated_system_msg, message, sizeof(g_accumulated_system_msg) - strlen(g_accumulated_system_msg) - 1);
    strncat(g_accumulated_system_msg, "\n", sizeof(g_accumulated_system_msg) - strlen(g_accumulated_system_msg) - 1);
    g_system_msg_count++;
}

static bool testFallbackCliHandler(const char* command, char* reply, size_t max_len) {
    if (strcmp(command, "get radio") == 0) {
        strncpy(reply, "> 869.618,62.500,8,5", max_len - 1);
        return true;
    }
    if (strncmp(command, "set name ", 9) == 0) {
        strncpy(reply, "OK", max_len - 1);
        return true;
    }
    return false;
}

static void testSendSosPacket(const char* channel, const char* target_contact, const char* message) {
    if (channel) {
        strncpy(g_last_sos_channel, channel, sizeof(g_last_sos_channel) - 1);
    }
    strncpy(g_last_sos_msg, message, sizeof(g_last_sos_msg) - 1);
}

static void testSendTelemetryPacket(const char* /*target_channel*/, const char* message) {
    strncpy(g_last_telemetry_msg, message, sizeof(g_last_telemetry_msg) - 1);
}

// ====================================================================
// Test Suite 1: Romanian Regions & AutoRegions Fallback
// ====================================================================
void run_test_regions() {
    printf("[TEST] Running Romanian Regions & AutoRegions Tests...\n");

    // 1. Point in polygon / distance tests
    // Bucharest centroid (approx 44.4323, 26.1063)
    const CountyEntry* b_county = findClosestCounty(44.4323f, 26.1063f);
    assert(b_county != nullptr);
    assert(strcasecmp(b_county->code, "B") == 0);
    assert(strcmp(b_county->macro, "ro-b-if") == 0);

    // Cluj centroid (approx 46.7712, 23.6236)
    const CountyEntry* cj_county = findClosestCounty(46.7712f, 23.6236f);
    assert(cj_county != nullptr);
    assert(strcasecmp(cj_county->code, "CJ") == 0);
    assert(strcmp(cj_county->macro, "ro-nord-vest") == 0);

    // Timisoara centroid (approx 45.7537, 21.2257)
    const CountyEntry* tm_county = findClosestCounty(45.7537f, 21.2257f);
    assert(tm_county != nullptr);
    assert(strcasecmp(tm_county->code, "TM") == 0);
    assert(strcmp(tm_county->macro, "ro-vest") == 0);

    // Brasov centroid (approx 45.6579, 25.6012)
    const CountyEntry* bv_county = findClosestCounty(45.6579f, 25.6012f);
    assert(bv_county != nullptr);
    assert(strcasecmp(bv_county->code, "BV") == 0);
    assert(strcmp(bv_county->macro, "ro-centru") == 0);

    // Iasi centroid (approx 47.1585, 27.6014)
    const CountyEntry* is_county = findClosestCounty(47.1585f, 27.6014f);
    assert(is_county != nullptr);
    assert(strcasecmp(is_county->code, "IS") == 0);
    assert(strcmp(is_county->macro, "ro-nord-est") == 0);

    // Constanta centroid (approx 44.1807, 28.6343)
    const CountyEntry* ct_county = findClosestCounty(44.1807f, 28.6343f);
    assert(ct_county != nullptr);
    assert(strcasecmp(ct_county->code, "CT") == 0);
    assert(strcmp(ct_county->macro, "ro-sud-est") == 0);

    // 2. Fallback prefix parsing from node names
    const CountyEntry* parsed_cj = findCountyFromNodeName("RO-CJ-REP-01");
    assert(parsed_cj != nullptr);
    assert(strcasecmp(parsed_cj->code, "CJ") == 0);

    const CountyEntry* parsed_b = findCountyFromNodeName("RO-B-01-NORD");
    assert(parsed_b != nullptr);
    assert(strcasecmp(parsed_b->code, "B") == 0);

    const CountyEntry* parsed_bv = findCountyFromNodeName("RO-BV-TAMPA");
    assert(parsed_bv != nullptr);
    assert(strcasecmp(parsed_bv->code, "BV") == 0);

    // 3. Non-conforming name fallback test
    const CountyEntry* non_conforming = findCountyFromNodeName("CustomNode123");
    assert(non_conforming == nullptr); // No county match

    // AutomaticRegions initialization with non-conforming name -> defaults to #ro
    AutomaticRegions::instance().init(0.0, 0.0, "MyStationBeta");
    assert(strcmp(AutomaticRegions::instance().getActiveScope(), "#ro") == 0);

    // AutomaticRegions with conforming name
    AutomaticRegions::instance().init(0.0, 0.0, "RO-TM-01");
    assert(strcmp(AutomaticRegions::instance().getActiveScope(), "ro-tm") == 0);

    printf("  -> Romanian Regions & AutoRegions tests passed!\n");
}

// ====================================================================
// Test Suite 2: Quarantine Engine & LoRa Airtime Token Bucket
// ====================================================================
void run_test_quarantine() {
    printf("[TEST] Running Quarantine & Airtime Bucket Tests...\n");

    QuarantineEngine::instance().init(3000, 250); // 3000ms max bucket, 250ms/sec fill rate

    // Verify Semtech airtime calculation (Narrow SF8 BW62.5 CR8)
    uint32_t at_32 = QuarantineEngine::calculateAirtimeMs(32);
    uint32_t at_128 = QuarantineEngine::calculateAirtimeMs(128);
    uint32_t at_256 = QuarantineEngine::calculateAirtimeMs(256);
    assert(at_32 >= 200 && at_32 <= 400);
    assert(at_128 > at_32);
    assert(at_256 > at_128);

    uint8_t dummy_pubkey[32] = {0x11, 0x22, 0x33, 0x44};

    // 1. Test 24-hour remote advert rate limiting
    uint32_t t0 = 1000;
    bool adv1 = QuarantineEngine::instance().recordRemoteAdvert(dummy_pubkey, t0);
    assert(adv1 == true); // First advert permitted

    // Advert 1 hour later (3600s = 3600000ms)
    bool adv2 = QuarantineEngine::instance().recordRemoteAdvert(dummy_pubkey, t0 + 3600000);
    assert(adv2 == false); // Suppressed (within 24h)

    // Advert 25 hours later (25 * 3600 * 1000 = 90,000,000ms)
    bool adv3 = QuarantineEngine::instance().recordRemoteAdvert(dummy_pubkey, t0 + 90000000);
    assert(adv3 == true); // Allowed (past 24h limit)

    // 2. Test Airtime Token Bucket & Auto-Quarantine
    uint8_t flooder_pubkey[32] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint32_t cur_ms = 100000;

    // Send rapid burst of packets exceeding token budget
    bool allowed = true;
    for (int i = 0; i < 15; i++) {
        allowed = QuarantineEngine::instance().recordPacket(flooder_pubkey, 400, cur_ms);
        cur_ms += 50; // Rapid succession (20 packets/sec)
    }
    // Should have exhausted tokens and triggered quarantine
    assert(allowed == false);
    assert(QuarantineEngine::instance().isBanned(flooder_pubkey, cur_ms) == true);

    // 3. Test Manual CLI ban commands
    char reply[128];
    bool ban_cmd = QuarantineEngine::instance().handleCommand("ban add aabbccdd 120", reply, sizeof(reply));
    assert(ban_cmd == true);

    bool del_cmd = QuarantineEngine::instance().handleCommand("ban del aabbccdd", reply, sizeof(reply));
    assert(del_cmd == true);

    printf("  -> Quarantine & Airtime tests passed!\n");
}

// ====================================================================
// Test Suite 3: Adaptive Tier Delays & Physics Timing
// ====================================================================
void run_test_tier() {
    printf("[TEST] Running Adaptive Tier Tests...\n");

    AutomaticTiers::instance().setMode(TierMode::Manual);
    
    // Tier 0 (Backbone mountain-top infrastructure)
    AutomaticTiers::instance().setManualTier(0);
    TierDelays d0 = AutomaticTiers::instance().getActiveDelays();
    assert(d0.rxdelay == 3.0f);
    assert(d0.txdelay == 2.0f);

    // Tier 3 (Local edge / dense suburban)
    AutomaticTiers::instance().setManualTier(3);
    TierDelays d3 = AutomaticTiers::instance().getActiveDelays();
    assert(d3.rxdelay == 3.0f);
    assert(d3.txdelay == 0.3f);

    // Verify delays are in seconds and physics compliant for Narrow LoRa
    assert(d0.txdelay > d3.txdelay);

    // CLI commands: get tier, set tier
    char reply[128];
    bool get_cmd = AutomaticTiers::instance().handleCommand("get tier", reply, sizeof(reply));
    assert(get_cmd == true);
    assert(strstr(reply, "T3") != nullptr);

    bool set_auto = AutomaticTiers::instance().handleCommand("set tier auto", reply, sizeof(reply));
    assert(set_auto == true);
    assert(AutomaticTiers::instance().getMode() == TierMode::Auto);

    bool set_off = AutomaticTiers::instance().handleCommand("set tier off", reply, sizeof(reply));
    assert(set_off == true);
    assert(AutomaticTiers::instance().getMode() == TierMode::Disabled);

    printf("  -> Adaptive Tier tests passed!\n");
}

// ====================================================================
// Test Suite 4: PA Gain Limiter
// ====================================================================
void run_test_pa_limiter() {
    printf("[TEST] Running PA Gain Limiter Tests...\n");

    GainEngine::instance().setLimiterEnabled(true);
    GainEngine::instance().setLegalLimit(22); // Max +22 dBm conducted ERP
    GainEngine::instance().setPaGain(14);     // +14 dB external PA (e.g. 1W board)

    // With target power +22 dBm and +14 dB PA, transceiver drive must be clamped to +8 dBm
    int8_t clamped_drive = GainEngine::instance().clampTransceiverDrive(22);
    assert(clamped_drive == 8);

    // Resulting effective conducted power: 8 + 14 = 22 dBm
    int8_t effective_out = GainEngine::instance().getEffectiveConductedPower(clamped_drive);
    assert(effective_out == 22);

    // When limiter is disabled, transceiver drive is not attenuated
    GainEngine::instance().setLimiterEnabled(false);
    assert(GainEngine::instance().clampTransceiverDrive(22) == 22);

    // CLI commands
    char reply[128];
    GainEngine::instance().handleCommand("set pa.limiter on", reply, sizeof(reply));
    assert(GainEngine::instance().isLimiterEnabled() == true);

    GainEngine::instance().handleCommand("set pa.gain 12", reply, sizeof(reply));
    assert(GainEngine::instance().getPaGain() == 12);

    printf("  -> PA Gain Limiter tests passed!\n");
}

// ====================================================================
// Test Suite 5: T1000-E SOS & Virtual System Channel CLI
// ====================================================================
void run_test_sos_and_cli() {
    printf("[TEST] Running T1000-E SOS & Channel CLI Tests...\n");

    // Initialize Virtual Contact & Channels
    CliAssistant::instance().init();
    CliAssistant::instance().setInjectCallback(testInjectSystemMessage);

    SosT1000e::instance().init();
    SosT1000e::instance().setSendCallback(testSendSosPacket);

    // 1. Virtual System Contact Verification
    assert(isSystemContact(SYSTEM_VIRTUAL_PUBKEY) == true);
    uint8_t other_pubkey[32] = {0x99, 0x88};
    assert(isSystemContact(other_pubkey) == false);

    // 2. Automated Welcome Message Verification
    CliAssistant::instance().sendWelcomeNotice();
    assert(strstr(g_accumulated_system_msg, "Welcome to RO-mesh") != nullptr);
    assert(strstr(g_accumulated_system_msg, "/help") != nullptr);

    // 3. Channel CLI Slash Commands & Paginated Help Verification
    g_accumulated_system_msg[0] = '\0';
    g_system_msg_count = 0;
    bool handled_help = CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "/help");
    assert(handled_help == true);
    assert(g_system_msg_count > 1); // Confirmed split into multiple [1/X] chunks
    assert(strstr(g_accumulated_system_msg, "[1/") != nullptr);
    assert(strstr(g_accumulated_system_msg, "/status") != nullptr);
    assert(strstr(g_accumulated_system_msg, "get radio") != nullptr);
    assert(strstr(g_accumulated_system_msg, "set radio") != nullptr);
    assert(strstr(g_accumulated_system_msg, "set name") != nullptr);
    assert(strstr(g_accumulated_system_msg, "reboot") != nullptr);

    // 4. Native Companion CLI Delegation (get & set commands)
    CliAssistant::instance().setFallbackHandler(testFallbackCliHandler);

    // Test native "get radio"
    bool handled_get_radio = CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "get radio");
    assert(handled_get_radio == true);
    assert(strstr(g_last_system_msg, "869.618") != nullptr);

    // Test slash-prefixed "/get radio"
    bool handled_slash_radio = CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "/get radio");
    assert(handled_slash_radio == true);
    assert(strstr(g_last_system_msg, "869.618") != nullptr);

    // Test native "set name"
    bool handled_set_name = CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "set name MyNode");
    assert(handled_set_name == true);
    assert(strstr(g_last_system_msg, "OK") != nullptr);

    // /status
    g_accumulated_system_msg[0] = '\0';
    bool handled_status = CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "/status");
    assert(handled_status == true);
    assert(strstr(g_accumulated_system_msg, "RO-mesh Status") != nullptr);
    assert(strstr(g_accumulated_system_msg, "#ro") != nullptr);

    // /battery
    CliAssistant::instance().setBattery(85, 3.95f);
    CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "/battery");
    assert(strstr(g_last_system_msg, "85%") != nullptr);

    // /info
    CliAssistant::instance().handleMessage(SYSTEM_VIRTUAL_PUBKEY, false, "/info");
    assert(strstr(g_last_system_msg, "focaldistorsion") != nullptr);

    // 4. Hardware Button SOS State Machine
    // Double click sequence: Press -> Release -> Press -> Release within 400ms
    uint32_t t = 1000;
    SosT1000e::instance().onButtonEvent(true, t);       // Click 1 down
    t += 100;
    SosT1000e::instance().onButtonEvent(false, t);      // Click 1 up
    t += 150;
    SosT1000e::instance().onButtonEvent(true, t);       // Click 2 down
    t += 100;
    SosT1000e::instance().onButtonEvent(false, t);      // Click 2 up

    // Verify armed countdown
    assert(SosT1000e::instance().isArmed() == true);
    assert(SosT1000e::instance().getState() == SosState::ARMED_COUNTDOWN);

    // Cancel via second double-click
    t += 500;
    SosT1000e::instance().onButtonEvent(true, t);       // Cancel Click 1 down
    t += 100;
    SosT1000e::instance().onButtonEvent(false, t);      // Cancel Click 1 up
    t += 150;
    SosT1000e::instance().onButtonEvent(true, t);       // Cancel Click 2 down
    t += 100;
    SosT1000e::instance().onButtonEvent(false, t);      // Cancel Click 2 up
    assert(SosT1000e::instance().getState() == SosState::RESOLVED);

    // Re-arm via double click to test long-press fallback cancellation
    t += 1000;
    SosT1000e::instance().onButtonEvent(true, t);       // Click 1 down
    t += 100;
    SosT1000e::instance().onButtonEvent(false, t);      // Click 1 up
    t += 150;
    SosT1000e::instance().onButtonEvent(true, t);       // Click 2 down
    t += 100;
    SosT1000e::instance().onButtonEvent(false, t);      // Click 2 up
    assert(SosT1000e::instance().isArmed() == true);

    // 5-second long hold gesture to cancel/disarm (safety fallback)
    t += 1000;
    SosT1000e::instance().onButtonEvent(true, t);       // Hold down
    t += 5100;                                         // 5.1s later
    SosT1000e::instance().onButtonEvent(false, t);      // Release

    // Verify disarmed
    assert(SosT1000e::instance().getState() == SosState::RESOLVED);

    // Trigger SOS via CLI and simulate countdown expiry
    SosT1000e::instance().triggerManualSos("CLI");
    assert(SosT1000e::instance().isArmed() == true);
    SosT1000e::instance().update(t);

    SosT1000e::instance().updateGps(45.6579, 25.6012, 650.0f, true); // Brasov coordinates
    SosT1000e::instance().updateBattery(90);

    // Advance time past 15 seconds countdown
    t += 16000;
    SosT1000e::instance().update(t);

    assert(SosT1000e::instance().isActive() == true);
    assert(strstr(g_last_sos_msg, "[EMERGENCY-SOS]") != nullptr);
    assert(strstr(g_last_sos_msg, "https://maps.google.com") != nullptr);

    // Cancel active SOS -> resolution packet
    SosT1000e::instance().cancelSos("Cleared");
    assert(strstr(g_last_sos_msg, "[SOS-RESOLVED]") != nullptr);

    printf("  -> T1000-E SOS & Channel CLI tests passed!\n");
}

// ====================================================================
// Test Suite 6: Battery Gate, Packet Filter & Telemetry Bar
// ====================================================================
void run_test_filter_and_battery() {
    printf("[TEST] Running Battery Gate & Packet Filter Tests...\n");

    BatteryGate::instance().init(3.40f, 3.60f);

    // At 4.0V, relay is allowed
    BatteryGate::instance().updateVoltage(4.0f);
    assert(BatteryGate::instance().isRelayAllowed() == true);

    // Drops to 3.35V -> tripped, relay paused
    BatteryGate::instance().updateVoltage(3.35f);
    assert(BatteryGate::instance().isRelayAllowed() == false);

    // Rises to 3.50V -> still paused (below resume threshold 3.60V)
    BatteryGate::instance().updateVoltage(3.50f);
    assert(BatteryGate::instance().isRelayAllowed() == false);

    // Rises to 3.65V -> recovered!
    BatteryGate::instance().updateVoltage(3.65f);
    assert(BatteryGate::instance().isRelayAllowed() == true);

    // FilterEngine evaluation
    FilterEngine::instance().init(7);
    uint8_t sender[32] = {0x01, 0x02, 0x03};

    // Normal packet: ALLOW
    FilterVerdict v1 = FilterEngine::instance().evaluate(sender, false, 2, 64, 1000);
    assert(v1 == FilterVerdict::ALLOW);

    // Packet with 8 hops (> max 7): DROP_HOP_LIMIT
    FilterVerdict v2 = FilterEngine::instance().evaluate(sender, false, 8, 64, 1000);
    assert(v2 == FilterVerdict::DROP_HOP_LIMIT);

    // Strict Telemetry Bar from Public Channel
    TelemetryBeacon::instance().init();
    TelemetryBeacon::instance().setTransmitCallback(testSendTelemetryPacket);
    bool allowed = TelemetryBeacon::instance().setTargetChannel("Public");
    assert(allowed == false); // Must be rejected and routed to #romania
    assert(strcmp(TelemetryBeacon::instance().getConfig().target_channel, "#romania") == 0);

    printf("  -> Battery Gate & Packet Filter tests passed!\n");
}

// ====================================================================
// Test Suite 7: Deterministic Adverts 23-Hour Slotting
// ====================================================================
void run_test_deterministic_adverts() {
    printf("[TEST] Running Deterministic Adverts Tests...\n");

    uint8_t node1[32] = {0x12, 0x34, 0x56, 0x78};
    uint8_t node2[32] = {0xAA, 0xBB, 0xCC, 0xDD};

    SmartAdverts::instance().init(node1);
    uint32_t slot1 = SmartAdverts::instance().getSlotOffsetSec();

    SmartAdverts::instance().init(node2);
    uint32_t slot2 = SmartAdverts::instance().getSlotOffsetSec();

    // Verify slots are within 23-hour window
    assert(slot1 < 23 * 3600);
    assert(slot2 < 23 * 3600);
    // Different keys yield staggered slots
    assert(slot1 != slot2);

    printf("  -> Deterministic Adverts tests passed!\n");
}

int main() {
    printf("====================================================\n");
    printf("  RO-mesh Firmware Native Verification Suite\n");
    printf("====================================================\n");

    run_test_regions();
    run_test_quarantine();
    run_test_tier();
    run_test_pa_limiter();
    run_test_sos_and_cli();
    run_test_filter_and_battery();
    run_test_deterministic_adverts();

    printf("====================================================\n");
    printf("  ALL TESTS PASSED SUCCESSFULLY! (100%% Green)\n");
    printf("====================================================\n");
    return 0;
}
