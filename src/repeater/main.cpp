#include <Arduino.h>
#include <Mesh.h>

#include "../ro_mesh/regions/AutoRegionsRO.h"
#include "../ro_mesh/hal/AdaptiveTier.h"
#include "../ro_mesh/hal/PaGainLimiter.h"
#include "../ro_mesh/filter/QuarantineEngine.h"
#include "../ro_mesh/filter/PacketFilter.h"
#include "../ro_mesh/filter/BatteryGate.h"
#include "../ro_mesh/switcher/EmergencyProfileSwitcher.h"
#include "../ro_mesh/advert/DeterministicAdverts.h"

// Hardware and board includes from MeshCore
#ifdef DISPLAY_CLASS
  #include <helpers/ui/DisplayDriver.h>
#endif

using namespace ro_mesh;

#ifndef ADVERT_NAME
  #define ADVERT_NAME "RO-mesh Repeater"
#endif

#ifndef LORA_TX_POWER
  #define LORA_TX_POWER 22
#endif

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- Starting RO-mesh Repeater ---");

  // 1. Initialize PA Gain Limiter
#ifdef PA_GAIN_DB
  PaGainLimiter::instance().setLimiterEnabled(true);
  PaGainLimiter::instance().setPaGain(PA_GAIN_DB);
  int8_t clamped_drive = PaGainLimiter::instance().clampTransceiverDrive(LORA_TX_POWER);
  Serial.printf("[RO-mesh] Hardware PA Gain: +%d dB | Clamped SX1262 Drive: +%d dBm\n",
                PA_GAIN_DB, clamped_drive);
#else
  PaGainLimiter::instance().setLimiterEnabled(false);
  PaGainLimiter::instance().setPaGain(0);
#endif

  // 2. Initialize Battery Protection Gate
  BatteryGate::instance().init(3.40f, 3.60f);

  // 3. Initialize Packet Filter & Quarantine Engine
  PacketFilter::instance().init(7); // Max 7 hops
  QuarantineEngine::instance().init(3000, 250); // Airtime bucket defaults

  // 4. Initialize AutoRegions for Romania (#ro)
  AutoRegionsRO::instance().init();
  // Evaluates node name (e.g. RO-BV-01) or lat/lon coordinates
  const char* active_scope = AutoRegionsRO::instance().getActiveScope();
  Serial.printf("[RO-mesh] Active Regional Scope: %s\n", active_scope);

  // 5. Initialize Adaptive Tier Delays
  AdaptiveTier::instance().setMode(TierMode::Auto);
  Serial.printf("[RO-mesh] Adaptive Tier initialized (Mode: Auto, Active: T%u)\n",
                AdaptiveTier::instance().getActiveTier());

  // 6. Initialize Emergency Profile Switcher
  EmergencyProfileSwitcher::instance().init();

  Serial.println("[RO-mesh] Repeater modules initialized successfully.");
}

void loop() {
  uint32_t now_ms = millis();
  uint32_t now_sec = now_ms / 1000;

  // Process adaptive tier sliding window metrics
  AdaptiveTier::instance().loop(now_ms);

  // Check emergency profile switcher schedule/expiration
  EmergencyProfileSwitcher::instance().update(now_ms);

  // Check self-originated deterministic adverts
  if (DeterministicAdverts::instance().shouldTransmitAdvert(now_sec)) {
    Serial.println("[RO-mesh] Transmitting scheduled deterministic node advert...");
    DeterministicAdverts::instance().recordAdvertTransmitted(now_sec);
  }

  // Handle local serial CLI commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      char reply[256];
      memset(reply, 0, sizeof(reply));

      if (AdaptiveTier::instance().handleCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (PaGainLimiter::instance().handleCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (QuarantineEngine::instance().handleCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (PacketFilter::instance().handleCliCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (cmd.equalsIgnoreCase("status")) {
        Serial.printf("[RO-mesh Status] Scope: %s | Tier: T%u | Battery: %.2fV (Relay: %s)\n",
                      AutoRegionsRO::instance().getActiveScope(),
                      AdaptiveTier::instance().getActiveTier(),
                      (double)BatteryGate::instance().getCurrentVoltage(),
                      BatteryGate::instance().isRelayAllowed() ? "OK" : "PAUSED");
      } else {
        Serial.println("[RO-mesh] Unknown command. Try: get tier, get filter.stats, ban list, status");
      }
    }
  }

  delay(20);
}

