#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#include "MyMesh.h"

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(board, display);
#endif

#ifdef ETHERNET_ENABLED
  #define ETHERNET_CLI_BANNER "MeshCore Repeater CLI"
  #include <helpers/nrf52/EthernetCLI.h>
#endif

// RO-mesh module includes
#include "../ro_mesh/regions/AutomaticRegions.h"
#include "../ro_mesh/hal/AutomaticTiers.h"
#include "../ro_mesh/hal/GainEngine.h"
#include "../ro_mesh/filter/QuarantineEngine.h"
#include "../ro_mesh/filter/FilterEngine.h"
#include "../ro_mesh/filter/BatteryGate.h"
#include "../ro_mesh/switcher/PresetEngine.h"
#include "../ro_mesh/advert/SmartAdverts.h"

using namespace ro_mesh;

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];
#ifdef ETHERNET_ENABLED
static char ethernet_command[160];
#endif

// For power saving
unsigned long POWERSAVING_FIRSTSLEEP_SECS = 120; // The first sleep (if enabled) from boot

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
static unsigned long userBtnDownAt = 0;
#define USER_BTN_HOLD_OFF_MILLIS 1500
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#if defined(MESH_DEBUG) && defined(NRF52_PLATFORM)
  // give some extra time for serial to settle so
  // boot debug messages can be seen on terminal
  delay(5000);
#endif

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("RO-mesh Repeater");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;
#ifdef ETHERNET_ENABLED
  ethernet_command[0] = 0;
#endif

  sensors.begin();

  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

#ifdef ETHERNET_ENABLED
  ethernet_start_task();
#endif

  // -------------------------------------------------------------
  // Audible Startup Chime
  // -------------------------------------------------------------
#if defined(PIN_BUZZER) && defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
  pinMode(PIN_BUZZER_EN, OUTPUT);
  digitalWrite(PIN_BUZZER_EN, HIGH);
  tone(PIN_BUZZER, 2093, 100); // C7
  delay(120);
  tone(PIN_BUZZER, 2637, 100); // E7
  delay(120);
  tone(PIN_BUZZER, 3136, 180); // G7
  delay(200);
  digitalWrite(PIN_BUZZER_EN, LOW);
#elif defined(PIN_BUZZER)
  tone(PIN_BUZZER, 2093, 100);
  delay(120);
  tone(PIN_BUZZER, 2637, 100);
  delay(120);
  tone(PIN_BUZZER, 3136, 180);
  delay(200);
#endif

  // -------------------------------------------------------------
  // Visual LED Startup Flash
  // -------------------------------------------------------------
#ifdef PIN_STATUS_LED
  pinMode(PIN_STATUS_LED, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    delay(60);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(60);
  }
#endif

  // -------------------------------------------------------------
  // RO-mesh Repeater Modules Initialization
  // -------------------------------------------------------------
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

  BatteryGate::instance().init(3.40f, 3.60f);
  FilterEngine::instance().init(7);
  QuarantineEngine::instance().init(3000, 250);
  AutomaticRegions::instance().init();
  AutomaticTiers::instance().setMode(TierMode::Auto);
  PresetEngine::instance().init();
  SmartAdverts::instance().init(the_mesh.self_id.pub_key);

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  board.onBootComplete();
  Serial.println("[RO-mesh] Repeater running.");
}

void loop() {
  // Handle Serial CLI
  int len = strlen(command);
  if (len >= (int)sizeof(command)) {
    command[0] = 0;
    len = 0;
  }
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
      Serial.print(c);
    }
    if (c == '\r') break;
  }
  if (len == sizeof(command)-1) {
    command[sizeof(command)-2] = '\r';
    command[sizeof(command)-1] = 0;
  }

  if (len > 0 && command[len - 1] == '\r') {
    Serial.print('\n');
    command[len - 1] = 0;
    char reply[160];
    reply[0] = 0;

    if (AutomaticTiers::instance().handleCommand(command, reply, sizeof(reply))) {
      Serial.print("  -> "); Serial.println(reply);
    } else if (GainEngine::instance().handleCommand(command, reply, sizeof(reply))) {
      Serial.print("  -> "); Serial.println(reply);
    } else if (QuarantineEngine::instance().handleCommand(command, reply, sizeof(reply))) {
      Serial.print("  -> "); Serial.println(reply);
    } else if (FilterEngine::instance().handleCliCommand(command, reply, sizeof(reply))) {
      Serial.print("  -> "); Serial.println(reply);
    } else {
#ifdef ETHERNET_ENABLED
      if (!ethernet_handle_command(command, reply)) {
        the_mesh.handleCommand(0, command, reply);
      }
#else
      the_mesh.handleCommand(0, command, reply);
#endif
      if (reply[0]) {
        Serial.print("  -> "); Serial.println(reply);
      }
    }

    command[0] = 0;
  }

  board.loop();

#ifdef ETHERNET_ENABLED
  ethernet_loop_maintain();
  if (ethernet_read_line(ethernet_command, sizeof(ethernet_command))) {
    char reply[160];
    reply[0] = 0;
    if (!ethernet_handle_command(ethernet_command, reply)) {
      the_mesh.handleCommand(0, ethernet_command, reply);
    }
    ethernet_send_reply(reply);
    ethernet_command[0] = 0;
  }
#endif

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_) && !defined(DISPLAY_CLASS)
  int btnState = digitalRead(PIN_USER_BTN);
  if (btnState == LOW) {
    if (userBtnDownAt == 0) {
      userBtnDownAt = millis();
    } else if ((unsigned long)(millis() - userBtnDownAt) >= USER_BTN_HOLD_OFF_MILLIS) {
      Serial.println("Powering off...");
      board.powerOff();
    }
  } else {
    userBtnDownAt = 0;
  }
#endif

  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();

  uint32_t now_ms = millis();
  uint32_t now_sec = now_ms / 1000;

  AutomaticTiers::instance().loop(now_ms);
  PresetEngine::instance().update(now_ms);

  if (SmartAdverts::instance().shouldTransmitAdvert(now_sec)) {
    the_mesh.sendSelfAdvertisement(16000, false);
    SmartAdverts::instance().recordAdvertTransmitted(now_sec);
  }

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif

  if (the_mesh.getNodePrefs()->powersaving_enabled && !the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0);
#else
    if (the_mesh.millisHasNowPassed(POWERSAVING_FIRSTSLEEP_SECS * 1000)) {
      board.sleep(30);
    }
#endif
  }
}
