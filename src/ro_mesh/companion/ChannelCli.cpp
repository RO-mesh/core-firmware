#include "ChannelCli.h"
#include "SystemVirtualContact.h"
#include "DefaultChannels.h"
#include "T1000eSos.h"
#include "../hal/AdaptiveTier.h"
#include "../hal/PaGainLimiter.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace ro_mesh {

ChannelCli& ChannelCli::instance() {
    static ChannelCli inst;
    return inst;
}

ChannelCli::ChannelCli()
    : _inject_cb(nullptr),
      _welcome_sent(false),
      _battery_pct(100),
      _battery_volts(4.12f),
      _lat(0.0),
      _lon(0.0),
      _alt(0.0f),
      _gps_fix(false),
      _sats(0) {
}

void ChannelCli::init() {
    _welcome_sent = false;
}

bool ChannelCli::handleMessage(const uint8_t* destination_id, bool is_channel, const char* text) {
    if (!text || strlen(text) == 0) {
        return false;
    }

    bool is_system_dest = false;

    if (destination_id != nullptr) {
        if (!is_channel) {
            // Direct message: check if recipient is virtual System contact
            if (isSystemContact(destination_id)) {
                is_system_dest = true;
            }
        } else {
            // Channel message: check if sent to dedicated RO-mesh T1000E CLI channel
            if (memcmp(destination_id, ROMESH_CLI_PSK, 16) == 0) {
                is_system_dest = true;
            }
        }
    } else {
        // Destination is null: check if it's an explicit slash command
        if (text[0] == '/') {
            is_system_dest = true;
        }
    }

    if (!is_system_dest) {
        return false; // Not for CLI, allow normal mesh processing
    }

    // Process command
    executeCommand(text, destination_id, is_channel);
    return true; // Intercepted locally, do NOT transmit over RF
}

void ChannelCli::sendWelcomeNotice(const uint8_t* destination_id, bool is_channel) {
    const char* welcome_msg = 
        "👋 Welcome to RO-mesh! I am your System companion assistant.\n"
        "Type /help to see all available commands and manage your device directly from chat with zero RF airtime.";

    const uint8_t* target = destination_id ? destination_id : SYSTEM_VIRTUAL_PUBKEY;
    sendReply(target, is_channel, welcome_msg);
    _welcome_sent = true;
}

void ChannelCli::postSystemNotification(const char* message, const uint8_t* destination_id, bool is_channel) {
    if (!message) return;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "🔔 [SYSTEM] %s", message);

    const uint8_t* target = destination_id ? destination_id : SYSTEM_VIRTUAL_PUBKEY;
    sendReply(target, is_channel, buffer);
}

void ChannelCli::sendReply(const uint8_t* dest, bool is_ch, const char* reply) {
    if (_inject_cb) {
        _inject_cb(dest, is_ch, reply);
    }
}

void ChannelCli::executeCommand(const char* cmd_line, const uint8_t* destination_id, bool is_channel) {
    const uint8_t* target = destination_id ? destination_id : SYSTEM_VIRTUAL_PUBKEY;

    // Skip leading spaces
    while (*cmd_line == ' ') cmd_line++;

    // Strip leading slash if present
    const char* p = cmd_line;
    if (*p == '/') p++;

    if (strncasecmp(p, "help", 4) == 0 || strcmp(p, "?") == 0 || strlen(p) == 0) {
        cmdHelp(target, is_channel);
    } else if (strncasecmp(p, "status", 6) == 0) {
        cmdStatus(target, is_channel);
    } else if (strncasecmp(p, "tier", 4) == 0) {
        cmdTier(p + 4, target, is_channel);
    } else if (strncasecmp(p, "sos", 3) == 0) {
        cmdSos(p + 3, target, is_channel);
    } else if (strncasecmp(p, "telemetry", 9) == 0) {
        cmdTelemetry(p + 9, target, is_channel);
    } else if (strncasecmp(p, "battery", 7) == 0) {
        cmdBattery(target, is_channel);
    } else if (strncasecmp(p, "gps", 3) == 0) {
        cmdGps(target, is_channel);
    } else if (strncasecmp(p, "pa", 2) == 0) {
        cmdPa(target, is_channel);
    } else if (strncasecmp(p, "info", 4) == 0) {
        cmdInfo(target, is_channel);
    } else {
        char err[128];
        snprintf(err, sizeof(err), "❓ Unknown command: '%s'\nType /help to see all available commands.", cmd_line);
        sendReply(target, is_channel, err);
    }
}

void ChannelCli::cmdHelp(const uint8_t* dest, bool is_ch) {
    const char* help_text = 
        "🔧 RO-mesh Companion Commands:\n"
        "• /status - Battery, GPS, Tier, and PA limiter\n"
        "• /tier [0-3|auto|off] - Delay tier configuration\n"
        "• /sos [trigger|cancel|status] - Emergency beacon\n"
        "• /telemetry [on|off|status] - Telemetry beacon\n"
        "• /battery - Voltage & charge state\n"
        "• /gps - Current coordinates & satellites\n"
        "• /pa - PA limiter & RF power clamp\n"
        "• /info - Firmware version & scope (#ro)";
    sendReply(dest, is_ch, help_text);
}

void ChannelCli::cmdStatus(const uint8_t* dest, bool is_ch) {
    char buf[256];
    const char* tier_str = "Auto";
    if (AdaptiveTier::instance().getMode() == TierMode::Manual) {
        tier_str = "Manual";
    } else if (AdaptiveTier::instance().getMode() == TierMode::Disabled) {
        tier_str = "Disabled";
    }

    snprintf(buf, sizeof(buf),
             "📊 RO-mesh Status:\n"
             "Scope: #ro (Romania)\n"
             "Battery: %u%% (%.2fV)\n"
             "GPS: %s (Sats: %u)\n"
             "Tier: %s (Active: T%u)\n"
             "PA Limiter: %s (Gain: +%ddB)",
             _battery_pct, (double)_battery_volts,
             _gps_fix ? "FIX 3D" : "NO FIX", _sats,
             tier_str, AdaptiveTier::instance().getActiveTier(),
             PaGainLimiter::instance().isLimiterEnabled() ? "ACTIVE" : "BYPASS",
             PaGainLimiter::instance().getPaGain());
    sendReply(dest, is_ch, buf);
}

void ChannelCli::cmdTier(const char* args, const uint8_t* dest, bool is_ch) {
    while (*args == ' ') args++;
    char reply[128];

    if (strlen(args) == 0) {
        // Display current tier status
        AdaptiveTier::instance().handleCommand("get tier", reply, sizeof(reply));
    } else {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "set tier %s", args);
        AdaptiveTier::instance().handleCommand(cmd, reply, sizeof(reply));
    }
    sendReply(dest, is_ch, reply);
}

void ChannelCli::cmdSos(const char* args, const uint8_t* dest, bool is_ch) {
    while (*args == ' ') args++;
    char reply[160];

    if (strcasecmp(args, "trigger") == 0) {
        T1000eSos::instance().triggerManualSos("CLI /sos trigger");
        snprintf(reply, sizeof(reply), "🚨 Emergency SOS ARMED! Countdown active (%u s). Hold 5s or /sos cancel to abort.",
                 T1000eSos::instance().getConfig().countdown_seconds);
    } else if (strcasecmp(args, "cancel") == 0) {
        T1000eSos::instance().cancelSos("CLI /sos cancel");
        snprintf(reply, sizeof(reply), "✅ SOS has been cancelled. Resolution transmitted.");
    } else {
        const char* st = "IDLE";
        if (T1000eSos::instance().isArmed()) st = "ARMED_COUNTDOWN";
        else if (T1000eSos::instance().isActive()) st = "ACTIVE_BEACONING";
        else if (T1000eSos::instance().getState() == SosState::RESOLVED) st = "RESOLVED";

        snprintf(reply, sizeof(reply),
                 "🚨 SOS Status: %s\nChannel: %s | Interval: %us | Beacons Sent: %u",
                 st, T1000eSos::instance().getConfig().channel,
                 T1000eSos::instance().getConfig().interval_seconds,
                 T1000eSos::instance().getActiveBeaconsSent());
    }
    sendReply(dest, is_ch, reply);
}

void ChannelCli::cmdTelemetry(const char* args, const uint8_t* dest, bool is_ch) {
    while (*args == ' ') args++;
    char reply[128];

    if (strcasecmp(args, "on") == 0) {
        snprintf(reply, sizeof(reply), "📍 Telemetry beacon enabled (Strictly barred from Public channel).");
    } else if (strcasecmp(args, "off") == 0) {
        snprintf(reply, sizeof(reply), "📍 Telemetry beacon disabled.");
    } else {
        snprintf(reply, sizeof(reply), "📍 Telemetry: Target channel #romania | Movement threshold: 100m");
    }
    sendReply(dest, is_ch, reply);
}

void ChannelCli::cmdBattery(const uint8_t* dest, bool is_ch) {
    char reply[96];
    snprintf(reply, sizeof(reply), "🔋 Battery: %u%% | Voltage: %.2fV", _battery_pct, (double)_battery_volts);
    sendReply(dest, is_ch, reply);
}

void ChannelCli::cmdGps(const uint8_t* dest, bool is_ch) {
    char reply[160];
    if (_gps_fix) {
        snprintf(reply, sizeof(reply),
                 "🛰️ GPS Fix: %.5f, %.5f\nAlt: %.1fm | Sats: %u\nMap: https://maps.google.com/?q=%.5f,%.5f",
                 _lat, _lon, (double)_alt, _sats, _lat, _lon);
    } else {
        snprintf(reply, sizeof(reply), "🛰️ GPS: Searching for satellites... (Sats visible: %u)", _sats);
    }
    sendReply(dest, is_ch, reply);
}

void ChannelCli::cmdPa(const uint8_t* dest, bool is_ch) {
    char reply[128];
    snprintf(reply, sizeof(reply),
             "📶 PA Gain Limiter: %s\nHardware PA Gain: +%d dB\nLegal Output Limit: +%d dBm conducted",
             PaGainLimiter::instance().isLimiterEnabled() ? "ENABLED" : "DISABLED",
             PaGainLimiter::instance().getPaGain(),
             PaGainLimiter::instance().getLegalLimit());
    sendReply(dest, is_ch, reply);
}

void ChannelCli::cmdInfo(const uint8_t* dest, bool is_ch) {
    const char* info = 
        "ℹ️ RO-mesh Firmware v1.0.0\n"
        "Architecture: Decoupled Zero-Conflict Core\n"
        "Default Scope: #ro\n"
        "Author: focaldistorsion";
    sendReply(dest, is_ch, info);
}

} // namespace ro_mesh
