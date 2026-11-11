#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#include "MyMesh.h"

// Believe it or not, this std C function is busted on some platforms!
static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

// interface manager
#include <helpers/MultiSerialInterface.h>
MultiSerialInterface interface_manager;

// include bluetooth interface
#if defined(BLE_PIN_CODE)
  #ifdef ESP32
    #include <helpers/esp32/SerialBLEInterface.h>
    SerialBLEInterface bluetooth_interface;
  #elif defined(NRF52_PLATFORM)
    #include <helpers/nrf52/SerialBLEInterface.h>
    SerialBLEInterface bluetooth_interface;
  #elif defined(RP2040_PLATFORM)
    #include <helpers/rp2040/SerialBLEInterface.h>
    SerialBLEInterface bluetooth_interface;
  #else
    #error "SerialBLEInterface is not defined for this platform"
  #endif
#endif

// include usb interface
#if defined(ENABLE_USB_INTERFACE)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface usb_serial_interface;
#endif

// platform file system
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
  #if defined(QSPIFLASH)
    #include <CustomLFS_QSPIFlash.h>
    DataStore store(InternalFS, QSPIFlash, rtc_clock);
  #else
    #if defined(EXTRAFS)
      #include <CustomLFS.h>
      CustomLFS ExtraFS(0xD4000, 0x19000, 128);
      DataStore store(InternalFS, ExtraFS, rtc_clock);
    #else
      DataStore store(InternalFS, rtc_clock);
    #endif
  #endif
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
  DataStore store(LittleFS, rtc_clock);
#elif defined(ESP32)
  #include <SPIFFS.h>
  DataStore store(SPIFFS, rtc_clock);
#endif

/* GLOBAL OBJECTS */
#ifdef DISPLAY_CLASS
  #include "UITask.h"
  UITask ui_task(&board, &interface_manager);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store);

void halt() {
  while (1) ;
}

// -------------------------------------------------------------
// RO-mesh Companion Extensions
// -------------------------------------------------------------
#include "../ro_mesh/companion/RomanianChannels.h"
#include "../ro_mesh/companion/CliContact.h"
#include "../ro_mesh/companion/CliAssistant.h"
#include "../ro_mesh/companion/SosT1000e.h"
#include "../ro_mesh/companion/TelemetryBeacon.h"
#include "../ro_mesh/companion/MessageStorage.h"

void setup() {
  Serial.begin(115200);
  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#ifdef DISPLAY_CLASS
  DisplayDriver* disp = NULL;
  if (display.begin()) {
    disp = &display;
    disp->startFrame();
    disp->drawTextCentered(disp->width() / 2, 28, "RO-mesh...");
    disp->endFrame();
  }
#endif

  if (!radio_init()) {
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  #if defined(EXTRAFS)
    ExtraFS.begin();
  #endif
  store.begin();
  the_mesh.begin();
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  store.begin();
  the_mesh.begin();
#elif defined(ESP32)
  SPIFFS.begin(true);
  store.begin();
  the_mesh.begin();
#endif

#ifdef BLE_PIN_CODE
  if (the_mesh.getNodePrefs()->ble_pin == 0) {
    the_mesh.setBLEPin(BLE_PIN_CODE);
  } else {
    the_mesh.setBLEPin(the_mesh.getNodePrefs()->ble_pin);
  }
#else
  the_mesh.setBLEPin(0);
#endif

#if defined(BLE_PIN_CODE)
  bluetooth_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
  interface_manager.addInterface(InterfaceType::Bluetooth, &bluetooth_interface);
#endif

#if defined(ENABLE_USB_INTERFACE)
  usb_serial_interface.begin(Serial);
  interface_manager.addInterface(InterfaceType::USB, &usb_serial_interface);
#endif

  the_mesh.startInterface(interface_manager);
  sensors.begin();

#if ENV_INCLUDE_GPS == 1
  the_mesh.applyGpsPrefs();
#endif

#ifdef DISPLAY_CLASS
  ui_task.begin(disp, &sensors, the_mesh.getNodePrefs());
  the_mesh.setListener(&ui_task);
#endif

  board.onBootComplete();

  // -------------------------------------------------------------
  // Audible Startup Chime on T1000-E Buzzer
  // -------------------------------------------------------------
#if defined(PIN_BUZZER)
  #if defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    pinMode(PIN_BUZZER_EN, OUTPUT);
    digitalWrite(PIN_BUZZER_EN, HIGH);
  #endif
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, 2093, 80); // C7
  delay(100);
  tone(PIN_BUZZER, 2637, 80); // E7
  delay(100);
  tone(PIN_BUZZER, 3136, 120); // G7
  delay(140);
  #if defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    digitalWrite(PIN_BUZZER_EN, LOW);
  #endif
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
  // RO-mesh Companion Extensions Initialization
  // -------------------------------------------------------------
  ro_mesh::MessageStorage::instance().init();

  ro_mesh::CliAssistant::instance().init();
  ro_mesh::CliAssistant::instance().setInjectCallback([](const uint8_t* target_id, bool is_channel, const char* message) {
    Serial.printf("[SYSTEM CHAT] Target: %s | Msg: %s\n", is_channel ? "Channel" : "System Contact", message);
    ro_mesh::MessageStorage::instance().addMessage(
      ro_mesh::SYSTEM_VIRTUAL_PUBKEY, "System", is_channel ? "CLI" : "", message, false, millis() / 1000
    );
  });

  ro_mesh::SosT1000e::instance().init();
  ro_mesh::SosT1000e::instance().setSendCallback([](const char* channel, const char* target_contact, const char* message) {
    Serial.printf("[SOS BROADCAST] %s: %s\n", channel ? channel : "Direct", message);
  });
  ro_mesh::SosT1000e::instance().setBeepCallback([](uint32_t freq_hz, uint32_t duration_ms) {
  #if defined(PIN_BUZZER) && defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    digitalWrite(PIN_BUZZER_EN, HIGH);
    tone(PIN_BUZZER, freq_hz, duration_ms);
    delay(duration_ms);
    digitalWrite(PIN_BUZZER_EN, LOW);
  #elif defined(PIN_BUZZER)
    tone(PIN_BUZZER, freq_hz, duration_ms);
    delay(duration_ms);
  #endif
  });

  ro_mesh::TelemetryBeacon::instance().init();
  ro_mesh::TelemetryBeacon::instance().setTransmitCallback([](const char* target_channel, const char* message) {
    Serial.printf("[TELEMETRY TX] Channel: %s | Msg: %s\n", target_channel, message);
  });

  ro_mesh::CliAssistant::instance().sendWelcomeNotice();
}

void loop() {
  the_mesh.loop();
  interface_manager.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();
  board.loop();
#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif

  // RO-mesh Companion Loop
  uint32_t now_ms = millis();
#ifdef PIN_USER_BTN
  bool btn_pressed = (digitalRead(PIN_USER_BTN) == USER_BTN_PRESSED);
  ro_mesh::SosT1000e::instance().onButtonEvent(btn_pressed, now_ms);
#endif
  ro_mesh::SosT1000e::instance().update(now_ms);
  ro_mesh::TelemetryBeacon::instance().update(now_ms);

  if (!the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#endif
  }
}
