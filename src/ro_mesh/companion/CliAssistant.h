#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

// Callback type to inject response into the incoming UI message stream
// target: channel hash or contact pubkey where the reply should appear
// is_channel: true if target is a channel hash, false if target is a 32-byte contact pubkey
typedef void (*InjectSystemMessageCallback)(const uint8_t* target_id, bool is_channel, const char* message);

class CliAssistant {
public:
    static CliAssistant& instance();

    void init();
    void setInjectCallback(InjectSystemMessageCallback cb) { _inject_cb = cb; }

    // Interception hook for incoming/outgoing messages
    // Returns true if the message was intercepted locally and should NOT be transmitted over RF
    bool handleMessage(const uint8_t* destination_id, bool is_channel, const char* text);

    // Automated welcome message trigger (e.g., when client app connects over BLE)
    void sendWelcomeNotice(const uint8_t* destination_id = nullptr, bool is_channel = false);

    // Post proactive asynchronous system event into the System chat
    void postSystemNotification(const char* message, const uint8_t* destination_id = nullptr, bool is_channel = false);

    // Dynamic state getters/setters for status command
    void setBattery(uint8_t pct, float voltage) { _battery_pct = pct; _battery_volts = voltage; }
    void setGps(double lat, double lon, float alt, bool fix, uint8_t sats) {
        _lat = lat; _lon = lon; _alt = alt; _gps_fix = fix; _sats = sats;
    }

private:
    CliAssistant();

    void executeCommand(const char* cmd_line, const uint8_t* destination_id, bool is_channel);
    void cmdHelp(const uint8_t* dest, bool is_ch);
    void cmdStatus(const uint8_t* dest, bool is_ch);
    void cmdTier(const char* args, const uint8_t* dest, bool is_ch);
    void cmdSos(const char* args, const uint8_t* dest, bool is_ch);
    void cmdTelemetry(const char* args, const uint8_t* dest, bool is_ch);
    void cmdBattery(const uint8_t* dest, bool is_ch);
    void cmdGps(const uint8_t* dest, bool is_ch);
    void cmdPa(const uint8_t* dest, bool is_ch);
    void cmdInfo(const uint8_t* dest, bool is_ch);

    void sendReply(const uint8_t* dest, bool is_ch, const char* reply);

    InjectSystemMessageCallback _inject_cb;
    bool _welcome_sent;

    uint8_t _battery_pct;
    float _battery_volts;
    double _lat;
    double _lon;
    float _alt;
    bool _gps_fix;
    uint8_t _sats;
};

} // namespace ro_mesh
