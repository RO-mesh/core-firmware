#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#include "MyMesh.h"

// -------------------------------------------------------------
// RO-mesh Companion Extensions
// -------------------------------------------------------------
#include "../ro_mesh/companion/RomanianChannels.h"
#include "../ro_mesh/companion/CliContact.h"
#include "../ro_mesh/companion/CliAssistant.h"
#include "../ro_mesh/companion/SosT1000e.h"
#include "../ro_mesh/companion/TelemetryBeacon.h"
#include "../ro_mesh/companion/MessageStorage.h"

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

class RoMeshSerialProxy : public BaseSerialInterface {
  BaseSerialInterface* _underlying = nullptr;

public:
  void setUnderlying(BaseSerialInterface* underlying) {
    _underlying = underlying;
  }

  void enable() override {
    if (_underlying) _underlying->enable();
  }

  void disable() override {
    if (_underlying) _underlying->disable();
  }

  bool isEnabled() const override {
    return _underlying && _underlying->isEnabled();
  }

  bool isConnected() const override {
    return _underlying && _underlying->isConnected();
  }

  void loop() override {
    if (_underlying) _underlying->loop();
  }

  bool isWriteBusy() const override {
    return _underlying && _underlying->isWriteBusy();
  }

  size_t writeFrame(const uint8_t src[], size_t len) override {
    if (!_underlying) return 0;
    return _underlying->writeFrame(src, len);
  }

  size_t checkRecvFrame(uint8_t dest[]) override {
    if (!_underlying) return 0;

    size_t len = _underlying->checkRecvFrame(dest);
    if (len <= 0) return 0;

    // Intercept CMD_SEND_TXT_MSG (0x02) to virtual System contact
    // Format: [0]=0x02, [1]=txt_type, [2]=attempt, [3..6]=timestamp, [7..12]=pub_key_prefix(6), [13..]=text
    if (dest[0] == 2 && len >= 14) {
      const uint8_t* pub_key_prefix = &dest[7];
      if (memcmp(pub_key_prefix, ro_mesh::SYSTEM_VIRTUAL_PUBKEY, 6) == 0) {
        char text_buf[160];
        size_t tlen = len - 13;
        if (tlen > sizeof(text_buf) - 1) tlen = sizeof(text_buf) - 1;
        memcpy(text_buf, &dest[13], tlen);
        text_buf[tlen] = '\0';

        // Calculate deterministic expected_ack for this message
        uint32_t expected_ack = 0;
        uint8_t temp[5 + 160];
        memcpy(temp, &dest[3], 4); // timestamp
        temp[4] = dest[2] & 3;     // attempt
        memcpy(&temp[5], &dest[13], tlen);
        mesh::Utils::sha256((uint8_t*)&expected_ack, 4, temp, 5 + tlen, ro_mesh::SYSTEM_VIRTUAL_PUBKEY, PUB_KEY_SIZE);
        if (expected_ack == 0) expected_ack = 0xA1B2C3D4;

        // 1. Send RESP_CODE_SENT (0x06) with expected_ack
        uint8_t out_frame[10];
        out_frame[0] = 6; // RESP_CODE_SENT
        out_frame[1] = 0; // direct
        uint32_t est_timeout = 100;
        memcpy(&out_frame[2], &expected_ack, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _underlying->writeFrame(out_frame, 10);

        // 2. Immediately send PUSH_CODE_SEND_CONFIRMED (0x82) to confirm delivery to app
        // This stops the official MeshCore app from retrying 8 times!
        uint8_t ack_frame[9];
        ack_frame[0] = 0x82; // PUSH_CODE_SEND_CONFIRMED
        memcpy(&ack_frame[1], &expected_ack, 4);
        uint32_t trip_time = 10; // 10 ms trip time
        memcpy(&ack_frame[5], &trip_time, 4);
        _underlying->writeFrame(ack_frame, 9);

        // Execute via CliAssistant
        ro_mesh::CliAssistant::instance().handleMessage(ro_mesh::SYSTEM_VIRTUAL_PUBKEY, false, text_buf);

        return 0; // Consumed
      }
    }

    return len;
  }
};
static RoMeshSerialProxy serial_proxy;

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

class RoMeshCompanionHelper : public MyMesh {
public:
  static void injectSystemMessage(const ContactInfo& from, const char* text) {
    mesh::Packet dummy_pkt;
    memset(&dummy_pkt, 0, sizeof(dummy_pkt));
    uint32_t now_ts = rtc_clock.getCurrentTimeUnique();
    static_cast<RoMeshCompanionHelper&>(the_mesh).queueMessage(from, TXT_TYPE_PLAIN, &dummy_pkt, now_ts, NULL, 0, text);
  }
};

static uint32_t s_buzzer_off_ms = 0;

void halt() {
  while (1) ;
}

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

  // -------------------------------------------------------------
  // Ensure Virtual System Contact & Default Romanian Channels
  // -------------------------------------------------------------
  ContactInfo* sys_contact = the_mesh.lookupContactByPubKey(ro_mesh::SYSTEM_VIRTUAL_PUBKEY, PUB_KEY_SIZE);
  if (!sys_contact) {
    ContactInfo sys;
    memset(&sys, 0, sizeof(ContactInfo));
    memcpy(sys.id.pub_key, ro_mesh::SYSTEM_VIRTUAL_PUBKEY, PUB_KEY_SIZE);
    strncpy(sys.name, "System", sizeof(sys.name) - 1);
    sys.type = 1; // ADV_TYPE_CHAT
    sys.flags = 0x01 | 0x10; // favorite + remote CLI allowed
    sys.out_path_len = 0; // direct local interface
    sys.lastmod = 1;
    the_mesh.addContact(sys);
    store.saveContacts(&the_mesh);
  }

#ifdef MAX_GROUP_CHANNELS
  ChannelDetails ch;
  if (!the_mesh.getChannel(0, ch)) {
    the_mesh.addChannel("#romania", "iLDLz3P0uM9sQKWjoO7DjQ==");
    the_mesh.addChannel("#sos", "lehpzs+tiL86CmVmy99vpg==");
    the_mesh.addChannel("#alerte", "2bIin6HPy1yM5u5lZ1VJDQ==");
    store.saveChannels(&the_mesh);
  }
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

  serial_proxy.setUnderlying(&interface_manager);
  the_mesh.startInterface(serial_proxy);
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

    ContactInfo* sys = the_mesh.lookupContactByPubKey(ro_mesh::SYSTEM_VIRTUAL_PUBKEY, PUB_KEY_SIZE);
    if (sys) {
      RoMeshCompanionHelper::injectSystemMessage(*sys, message);
    }
  });

  ro_mesh::CliAssistant::instance().setFallbackHandler([](const char* command, char* reply, size_t max_len) -> bool {
    while (*command == ' ') command++;

    // Radio prefs: get/set radio, get/set af, get/set dutycycle, get/set tx, etc.
    if (the_mesh.getNodePrefs()->getRadioPrefs()->handleCommand(command, 0, reply)) {
      if (the_mesh.getNodePrefs()->getRadioPrefs()->isDirty()) {
        the_mesh.savePrefs();
      }
      return true;
    }

    // Board hardware specific commands
    if (board.handleCommand(command, 0, reply)) {
      if (the_mesh.getNodePrefs()->isDirty()) {
        the_mesh.savePrefs();
      }
      return true;
    }

    if (strcmp(command, "reboot") == 0) {
      board.reboot();
      return true;
    }

    if (strcmp(command, "poweroff") == 0 || strcmp(command, "shutdown") == 0) {
      board.powerOff();
      return true;
    }

    if (memcmp(command, "set name ", 9) == 0) {
      if (AdvertDataParser::isValidName(&command[9])) {
        StrHelper::strncpy(the_mesh.getNodePrefs()->node_name, &command[9], sizeof(the_mesh.getNodePrefs()->node_name));
        the_mesh.savePrefs();
        strcpy(reply, "OK");
      } else {
        strcpy(reply, "Error, bad chars");
      }
      return true;
    }

    if (strcmp(command, "get name") == 0) {
      snprintf(reply, max_len, "> %s", the_mesh.getNodePrefs()->node_name);
      return true;
    }

    if (memcmp(command, "set pin ", 8) == 0) {
      the_mesh.getNodePrefs()->ble_pin = atoi(&command[8]);
      the_mesh.savePrefs();
      snprintf(reply, max_len, "> pin is now %06d", (int)the_mesh.getNodePrefs()->ble_pin);
      return true;
    }

    if (strcmp(command, "get pin") == 0) {
      snprintf(reply, max_len, "> %06d", (int)the_mesh.getNodePrefs()->ble_pin);
      return true;
    }

    if (strcmp(command, "board") == 0) {
      strncpy(reply, board.getManufacturerName(), max_len - 1);
      reply[max_len - 1] = '\0';
      return true;
    }

    if (strcmp(command, "ver") == 0) {
      snprintf(reply, max_len, "%s (Build: %s)", FIRMWARE_VERSION, FIRMWARE_BUILD_DATE);
      return true;
    }

    if (strcmp(command, "get tz.offset") == 0) {
      snprintf(reply, max_len, "> %d", the_mesh.getNodePrefs()->tz_offset);
      return true;
    }

    if (memcmp(command, "set tz.offset ", 14) == 0) {
      int8_t tz = atof(&command[14]);
      if (tz < -12 || tz > 14) {
        strcpy(reply, "Error, must be from -12 to +14");
      } else {
        the_mesh.getNodePrefs()->tz_offset = tz;
        the_mesh.savePrefs();
        strcpy(reply, "OK");
      }
      return true;
    }

    return false;
  });

  ro_mesh::SosT1000e::instance().init();
  ro_mesh::SosT1000e::instance().setSendCallback([](const char* channel, const char* target_contact, const char* message) {
    Serial.printf("[SOS BROADCAST] %s: %s\n", channel ? channel : "Direct", message);
  });

  ro_mesh::SosT1000e::instance().setBeepCallback([](uint32_t freq_hz, uint32_t duration_ms) {
  #if defined(PIN_BUZZER) && defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    digitalWrite(PIN_BUZZER_EN, HIGH);
    tone(PIN_BUZZER, freq_hz, duration_ms);
    s_buzzer_off_ms = millis() + duration_ms;
  #elif defined(PIN_BUZZER)
    tone(PIN_BUZZER, freq_hz, duration_ms);
    s_buzzer_off_ms = millis() + duration_ms;
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

  // Non-blocking buzzer auto-shutoff
  if (s_buzzer_off_ms > 0 && now_ms >= s_buzzer_off_ms) {
  #if defined(PIN_BUZZER)
    noTone(PIN_BUZZER);
  #endif
  #if defined(PIN_BUZZER_EN) && (PIN_BUZZER_EN >= 0)
    digitalWrite(PIN_BUZZER_EN, LOW);
  #endif
    s_buzzer_off_ms = 0;
  }

  if (!the_mesh.hasPendingWork() && s_buzzer_off_ms == 0) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#endif
  }
}
