#include <Arduino.h>
#include <Mesh.h>

#include "../ro_mesh/companion/DefaultChannels.h"
#include "../ro_mesh/companion/SystemVirtualContact.h"
#include "../ro_mesh/companion/ChannelCli.h"
#include "../ro_mesh/companion/T1000eSos.h"
#include "../ro_mesh/companion/TelemetryBeacon.h"
#include "../ro_mesh/companion/MessageStore.h"
#include "../ro_mesh/hal/PaGainLimiter.h"
#include "../ro_mesh/hal/AdaptiveTier.h"

using namespace ro_mesh;

#ifndef PIN_USER_BTN
  #define PIN_USER_BTN 0
#endif

#ifndef USER_BTN_PRESSED
  #define USER_BTN_PRESSED LOW
#endif

#ifdef PIN_BUZZER
  #ifndef PIN_BUZZER_EN
    #define PIN_BUZZER_EN -1
  #endif
#endif

// System message injection callback: delivers response to UI/chat interface
static void injectSystemMessage(const uint8_t* target_id, bool is_channel, const char* message) {
  Serial.printf("[SYSTEM CHAT] Target: %s | Msg: %s\n",
                is_channel ? "Channel" : "System Contact", message);
  // Store in offline message store
  MessageStore::instance().addMessage(
    SYSTEM_VIRTUAL_PUBKEY, "System", is_channel ? "CLI" : "", message, false, millis() / 1000
  );
}

// SOS transmit callback
static void sendSosPacket(const char* channel, const char* target_contact, const char* message) {
  if (channel) {
    Serial.printf("[SOS BROADCAST] Channel: %s | Payload: %s\n", channel, message);
  } else if (target_contact) {
    Serial.printf("[SOS DIRECT] Contact: %s | Payload: %s\n", target_contact, message);
  }
}

// Buzzer alert callback for countdown and warnings
static void beepSosAlert(uint32_t freq_hz, uint32_t duration_ms) {
#ifdef PIN_BUZZER
  #if defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    digitalWrite(PIN_BUZZER_EN, HIGH);
  #endif
  tone(PIN_BUZZER, freq_hz, duration_ms);
  delay(duration_ms);
  #if defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    digitalWrite(PIN_BUZZER_EN, LOW);
  #endif
#else
  Serial.printf("[BEEP] %u Hz for %u ms\n", freq_hz, duration_ms);
#endif
}

// Telemetry transmit callback
static void sendTelemetryPacket(const char* target_channel, const char* message) {
  Serial.printf("[TELEMETRY TX] Channel: %s | Msg: %s\n", target_channel, message);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- Starting RO-mesh Companion ---");

#if defined(PIN_BUZZER) && defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
  pinMode(PIN_BUZZER_EN, OUTPUT);
  digitalWrite(PIN_BUZZER_EN, LOW);
#endif

  pinMode(PIN_USER_BTN, INPUT_PULLUP);

  // 1. Initialize Message Store
  MessageStore::instance().init();

  // 2. Initialize Channel CLI with System injection callback
  ChannelCli::instance().init();
  ChannelCli::instance().setInjectCallback(injectSystemMessage);

  // 3. Initialize SOS Engine
  T1000eSos::instance().init();
  T1000eSos::instance().setSendCallback(sendSosPacket);
  T1000eSos::instance().setBeepCallback(beepSosAlert);

  // 4. Initialize Telemetry Beacon (Strictly barred from Public channel)
  TelemetryBeacon::instance().init();
  TelemetryBeacon::instance().setTransmitCallback(sendTelemetryPacket);

  // 5. Send automated welcome message to System chat
  ChannelCli::instance().sendWelcomeNotice();

  Serial.println("[RO-mesh] Companion ready. System virtual contact active.");
}

void loop() {
  uint32_t now_ms = millis();

  // 1. Hardware button handling (Double-press arm SOS, 5s hold cancel SOS)
  bool btn_pressed = (digitalRead(PIN_USER_BTN) == USER_BTN_PRESSED);
  T1000eSos::instance().onButtonEvent(btn_pressed, now_ms);

  // 2. SOS and Telemetry updates
  T1000eSos::instance().update(now_ms);
  TelemetryBeacon::instance().update(now_ms);

  // 3. Handle Serial input / chat simulation
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      // Pass directly to ChannelCli (handles /slash commands and chat messages)
      bool handled = ChannelCli::instance().handleMessage(nullptr, false, line.c_str());
      if (!handled) {
        Serial.printf("[CHAT ECHO] %s\n", line.c_str());
      }
    }
  }

  delay(20);
}

