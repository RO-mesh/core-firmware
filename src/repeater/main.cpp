#include <Arduino.h>
#include <Mesh.h>

#include "../ro_mesh/regions/AutomaticRegions.h"
#include "../ro_mesh/hal/AutomaticTiers.h"
#include "../ro_mesh/hal/GainEngine.h"
#include "../ro_mesh/filter/QuarantineEngine.h"
#include "../ro_mesh/filter/FilterEngine.h"
#include "../ro_mesh/filter/BatteryGate.h"
#include "../ro_mesh/switcher/PresetEngine.h"
#include "../ro_mesh/advert/SmartAdverts.h"

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

  // 1. Initialize Gain Engine
#ifdef PA_GAIN_DB
  GainEngine::instance().setLimiterEnabled(true);
  GainEngine::instance().setPaGain(PA_GAIN_DB);
  int8_t clamped_drive = GainEngine::instance().clampTransceiverDrive(LORA_TX_POWER);
  Serial.printf("[RO-mesh] Hardware PA Gain: +%d dB | Clamped SX1262 Drive: +%d dBm\n",
                PA_GAIN_DB, clamped_drive);
#else
  GainEngine::instance().setLimiterEnabled(false);
  GainEngine::instance().setPaGain(0);
#endif

  // 2. Initialize Battery Protection Gate
  BatteryGate::instance().init(3.40f, 3.60f);

  // 3. Initialize Filter Engine & Quarantine Engine
  FilterEngine::instance().init(7); // Max 7 hops
  QuarantineEngine::instance().init(3000, 250); // Airtime bucket defaults

  // 4. Initialize AutomaticRegions for Romania (#ro)
  AutomaticRegions::instance().init();
  // Evaluates node name (e.g. RO-BV-01) or lat/lon coordinates
  const char* active_scope = AutomaticRegions::instance().getActiveScope();
  Serial.printf("[RO-mesh] Active Regional Scope: %s\n", active_scope);

  // 5. Initialize Automatic Tiers Delays
  AutomaticTiers::instance().setMode(TierMode::Auto);
  Serial.printf("[RO-mesh] Automatic Tiers initialized (Mode: Auto, Active: T%u)\n",
                AutomaticTiers::instance().getActiveTier());

  // 6. Initialize Preset Engine
  PresetEngine::instance().init();

  Serial.println("[RO-mesh] Repeater modules initialized successfully.");
}

void loop() {
  uint32_t now_ms = millis();
  uint32_t now_sec = now_ms / 1000;

  // Process automatic tier sliding window metrics
  AutomaticTiers::instance().loop(now_ms);

  // Check preset engine schedule/expiration
  PresetEngine::instance().update(now_ms);

  // Check self-originated smart adverts
  if (SmartAdverts::instance().shouldTransmitAdvert(now_sec)) {
    Serial.println("[RO-mesh] Transmitting scheduled smart node advert...");
    SmartAdverts::instance().recordAdvertTransmitted(now_sec);
  }

  // Handle local serial CLI commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      char reply[256];
      memset(reply, 0, sizeof(reply));

      if (AutomaticTiers::instance().handleCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (GainEngine::instance().handleCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (QuarantineEngine::instance().handleCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (FilterEngine::instance().handleCliCommand(cmd.c_str(), reply, sizeof(reply))) {
        Serial.println(reply);
      } else if (cmd.equalsIgnoreCase("status")) {
        Serial.printf("[RO-mesh Status] Scope: %s | Tier: T%u | Battery: %.2fV (Relay: %s)\n",
                      AutomaticRegions::instance().getActiveScope(),
                      AutomaticTiers::instance().getActiveTier(),
                      (double)BatteryGate::instance().getCurrentVoltage(),
                      BatteryGate::instance().isRelayAllowed() ? "OK" : "PAUSED");
      } else {
        Serial.println("[RO-mesh] Unknown command. Try: get tier, get filter.stats, ban list, status");
      }
    }
  }

  delay(20);
}

