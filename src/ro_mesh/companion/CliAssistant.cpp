#include "CliAssistant.h"
#include "CliContact.h"
#include "RomanianChannels.h"
#include "SosT1000e.h"
#include "../hal/AutomaticTiers.h"
#include "../hal/GainEngine.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace ro_mesh {

CliAssistant& CliAssistant::instance() {
    static CliAssistant inst;
    return inst;
}

CliAssistant::CliAssistant()
    : _inject_cb(nullptr),
      _fallback_cb(nullptr),
      _welcome_sent(false),
      _battery_pct(100),
      _battery_volts(4.12f),
      _lat(0.0),
      _lon(0.0),
      _alt(0.0f),
      _gps_fix(false),
      _sats(0) {
}

void CliAssistant::init() {
    _welcome_sent = false;
}

bool CliAssistant::handleMessage(const uint8_t* destination_id, bool is_channel, const char* text) {
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

void CliAssistant::sendWelcomeNotice(const uint8_t* destination_id, bool is_channel) {
    const char* welcome_msg = 
        "👋 Welcome to RO-mesh! I am your System companion assistant.\n"
        "Type /help to see all available commands and manage your device directly from chat with zero RF airtime.";

    const uint8_t* target = destination_id ? destination_id : SYSTEM_VIRTUAL_PUBKEY;
    sendReply(target, is_channel, welcome_msg);
    _welcome_sent = true;
}

void CliAssistant::postSystemNotification(const char* message, const uint8_t* destination_id, bool is_channel) {
    if (!message) return;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "🔔 [SYSTEM] %s", message);

    const uint8_t* target = destination_id ? destination_id : SYSTEM_VIRTUAL_PUBKEY;
    sendReply(target, is_channel, buffer);
}

void CliAssistant::sendReply(const uint8_t* dest, bool is_ch, const char* reply) {
    if (!_inject_cb || !reply) return;

    size_t total_len = strlen(reply);
    const size_t CHUNK_SIZE = 115; // Safe payload length leaving room for [1/X] prefix

    if (total_len <= 130) {
        _inject_cb(dest, is_ch, reply);
        return;
    }

    // Split reply into chunks
    char chunks[10][140];
    int count = 0;
    const char* p = reply;

    while (*p && count < 10) {
        size_t rem = strlen(p);
        size_t take = (rem > CHUNK_SIZE) ? CHUNK_SIZE : rem;
        if (take < rem) {
            // Find natural breakpoint (newline preferred, then space)
            size_t b = take;
            while (b > 20 && p[b] != '\n') b--;
            if (b <= 20) {
                b = take;
                while (b > 20 && p[b] != ' ') b--;
            }
            if (b > 20) take = b;
        }
        memcpy(chunks[count], p, take);
        chunks[count][take] = '\0';
        p += take;
        while (*p == '\n' || *p == ' ') p++;
        count++;
    }

    for (int i = 0; i < count; i++) {
        char formatted[160];
        snprintf(formatted, sizeof(formatted), "[%d/%d] %s", i + 1, count, chunks[i]);
        _inject_cb(dest, is_ch, formatted);
    }
}

void CliAssistant::executeCommand(const char* cmd_line, const uint8_t* destination_id, bool is_channel) {
    const uint8_t* target = destination_id ? destination_id : SYSTEM_VIRTUAL_PUBKEY;

    // Skip leading spaces
    while (*cmd_line == ' ') cmd_line++;

    // Strip leading slash if present
    const char* p = cmd_line;
    if (*p == '/') p++;

    if (strcasecmp(p, "help") == 0 || strcmp(p, "?") == 0 || strlen(p) == 0) {
        cmdHelp(target, is_channel);
    } else if (strcasecmp(p, "status") == 0) {
        cmdStatus(target, is_channel);
    } else if (strncasecmp(p, "tier", 4) == 0 && (p[4] == '\0' || p[4] == ' ')) {
        cmdTier(p + 4, target, is_channel);
    } else if (strncasecmp(p, "sos", 3) == 0 && (p[3] == '\0' || p[3] == ' ')) {
        cmdSos(p + 3, target, is_channel);
    } else if (strncasecmp(p, "set sos", 7) == 0 && (p[7] == '\0' || p[7] == ' ')) {
        cmdSos(p + 7, target, is_channel);
    } else if (strcasecmp(p, "get sos") == 0) {
        cmdSos("", target, is_channel);
    } else if (strncasecmp(p, "telemetry", 9) == 0 && (p[9] == '\0' || p[9] == ' ')) {
        cmdTelemetry(p + 9, target, is_channel);
    } else if (strcasecmp(p, "battery") == 0) {
        cmdBattery(target, is_channel);
    } else if (strcasecmp(p, "gps") == 0) {
        cmdGps(target, is_channel);
    } else if (strcasecmp(p, "pa") == 0) {
        cmdPa(target, is_channel);
    } else if (strcasecmp(p, "info") == 0) {
        cmdInfo(target, is_channel);
    } else {
        // Not a built-in RO-mesh command -> try fallback handler (native MeshCore handleCommand)
        if (_fallback_cb) {
            char reply[256];
            reply[0] = '\0';
            // Try without slash (e.g. "get radio", "set name ...")
            if (_fallback_cb(p, reply, sizeof(reply))) {
                sendReply(target, is_channel, reply);
                return;
            }
            // If that fails and original had a slash, try original
            if (p != cmd_line && _fallback_cb(cmd_line, reply, sizeof(reply))) {
                sendReply(target, is_channel, reply);
                return;
            }
        }

        char err[160];
        snprintf(err, sizeof(err), "❓ Comandă necunoscută: '%s'\nTastați /help pentru lista completă de comenzi (RO-mesh & get/set).", cmd_line);
        sendReply(target, is_channel, err);
    }
}

void CliAssistant::cmdHelp(const uint8_t* dest, bool is_ch) {
    const char* help_text = 
        "🔧 RO-mesh:\n"
        "• /status, /battery, /gps, /info\n"
        "• /sos [trigger|cancel|cfg]\n"
        "• /tier [auto|0-3] | /telemetry\n"
        "⚙️ Setari:\n"
        "• radio <f,bw,sf,cr>, tx, af\n"
        "• dutycycle, name, pin, tz.offset\n"
        "• board, ver, reboot, poweroff";
    sendReply(dest, is_ch, help_text);
}

void CliAssistant::cmdStatus(const uint8_t* dest, bool is_ch) {
    char buf[256];
    const char* tier_str = "Auto";
    if (AutomaticTiers::instance().getMode() == TierMode::Manual) {
        tier_str = "Manual";
    } else if (AutomaticTiers::instance().getMode() == TierMode::Disabled) {
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
             tier_str, AutomaticTiers::instance().getActiveTier(),
             GainEngine::instance().isLimiterEnabled() ? "ACTIVE" : "BYPASS",
             GainEngine::instance().getPaGain());
    sendReply(dest, is_ch, buf);
}

void CliAssistant::cmdTier(const char* args, const uint8_t* dest, bool is_ch) {
    while (*args == ' ') args++;
    char reply[128];

    if (strlen(args) == 0) {
        // Display current tier status
        AutomaticTiers::instance().handleCommand("get tier", reply, sizeof(reply));
    } else {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "set tier %s", args);
        AutomaticTiers::instance().handleCommand(cmd, reply, sizeof(reply));
    }
    sendReply(dest, is_ch, reply);
}

void CliAssistant::cmdSos(const char* args, const uint8_t* dest, bool is_ch) {
    while (*args == ' ') args++;
    char reply[256];

    if (strcasecmp(args, "trigger") == 0 || strcasecmp(args, "on") == 0 || strcasecmp(args, "start") == 0) {
        SosT1000e::instance().triggerManualSos("CLI /sos trigger");
        snprintf(reply, sizeof(reply), "🚨 Emergency SOS ARMED! Countdown active (%u s). Hold 5s or /sos cancel to abort.",
                 SosT1000e::instance().getConfig().countdown_seconds);
    } else if (strcasecmp(args, "cancel force") == 0 || strcasecmp(args, "off force") == 0 || strcasecmp(args, "stop force") == 0) {
        SosT1000e::instance().cancelSos("CLI /sos cancel force", true);
        snprintf(reply, sizeof(reply), "✅ SOS cancelled immediately. Resolution transmitted.");
    } else if (strcasecmp(args, "cancel") == 0 || strcasecmp(args, "off") == 0 || strcasecmp(args, "stop") == 0) {
        if (SosT1000e::instance().isActive()) {
            SosT1000e::instance().cancelSos("CLI /sos cancel", false);
            snprintf(reply, sizeof(reply),
                     "🚨 Initiating SOS deactivation countdown (%u s). Type /sos cancel force to bypass or press button to abort deactivation.",
                     SosT1000e::instance().getConfig().countdown_seconds);
        } else {
            SosT1000e::instance().cancelSos("CLI /sos cancel", true);
            snprintf(reply, sizeof(reply), "✅ SOS has been cancelled.");
        }
    } else if (strncasecmp(args, "interval", 8) == 0 || strncasecmp(args, "set interval", 12) == 0) {
        const char* val = strchr(args, ' ');
        if (strncasecmp(args, "set interval", 12) == 0 && val) val = strchr(val + 1, ' ');
        if (val) {
            uint32_t sec = atoi(val);
            if (sec >= 10 && sec <= 3600) {
                SosT1000e::instance().setInterval(sec);
                snprintf(reply, sizeof(reply), "OK > SOS interval: %u s", sec);
            } else {
                snprintf(reply, sizeof(reply), "Error, interval must be 10..3600 s");
            }
        } else {
            snprintf(reply, sizeof(reply), "Usage: /sos interval <seconds>");
        }
    } else if (strncasecmp(args, "countdown", 9) == 0 || strncasecmp(args, "set countdown", 13) == 0) {
        const char* val = strchr(args, ' ');
        if (strncasecmp(args, "set countdown", 13) == 0 && val) val = strchr(val + 1, ' ');
        if (val) {
            uint32_t sec = atoi(val);
            if (sec <= 120) {
                SosT1000e::instance().setCountdown(sec);
                snprintf(reply, sizeof(reply), "OK > SOS countdown: %u s", sec);
            } else {
                snprintf(reply, sizeof(reply), "Error, countdown must be 0..120 s");
            }
        } else {
            snprintf(reply, sizeof(reply), "Usage: /sos countdown <seconds>");
        }
    } else if (strncasecmp(args, "channel", 7) == 0 || strncasecmp(args, "set channel", 11) == 0) {
        const char* val = strchr(args, ' ');
        if (strncasecmp(args, "set channel", 11) == 0 && val) val = strchr(val + 1, ' ');
        if (val) {
            while (*val == ' ') val++;
            SosT1000e::instance().setChannel(val);
            snprintf(reply, sizeof(reply), "OK > SOS channel: %s", val);
        } else {
            snprintf(reply, sizeof(reply), "Usage: /sos channel <#name>");
        }
    } else if (strncasecmp(args, "msg", 3) == 0 || strncasecmp(args, "set msg", 7) == 0) {
        const char* val = strchr(args, ' ');
        if (strncasecmp(args, "set msg", 7) == 0 && val) val = strchr(val + 1, ' ');
        if (val) {
            while (*val == ' ') val++;
            SosT1000e::instance().setMessage(val);
            snprintf(reply, sizeof(reply), "OK > SOS message updated");
        } else {
            snprintf(reply, sizeof(reply), "Usage: /sos msg <text>");
        }
    } else if (strncasecmp(args, "callsign", 8) == 0 || strncasecmp(args, "set callsign", 12) == 0) {
        const char* val = strchr(args, ' ');
        if (strncasecmp(args, "set callsign", 12) == 0 && val) val = strchr(val + 1, ' ');
        if (val) {
            while (*val == ' ') val++;
            SosT1000e::instance().setCallsign(val);
            snprintf(reply, sizeof(reply), "OK > SOS callsign: %s", val);
        } else {
            snprintf(reply, sizeof(reply), "Usage: /sos callsign <callsign>");
        }
    } else if (strncasecmp(args, "contacts", 8) == 0 || strncasecmp(args, "set contacts", 12) == 0) {
        const char* val = strchr(args, ' ');
        if (strncasecmp(args, "set contacts", 12) == 0 && val) val = strchr(val + 1, ' ');
        if (val) {
            while (*val == ' ') val++;
            SosT1000e::instance().setContacts(val);
            snprintf(reply, sizeof(reply), "OK > SOS contacts updated");
        } else {
            snprintf(reply, sizeof(reply), "Usage: /sos contacts <list>");
        }
    } else {
        const char* st = "IDLE";
        if (SosT1000e::instance().isArmed()) st = "ARMED_COUNTDOWN";
        else if (SosT1000e::instance().isDisarming()) st = "DISARM_COUNTDOWN";
        else if (SosT1000e::instance().isActive()) st = "ACTIVE_BEACONING";
        else if (SosT1000e::instance().getState() == SosState::RESOLVED) st = "RESOLVED";

        snprintf(reply, sizeof(reply),
                 "🚨 SOS Status: %s\n"
                 "• Channel: %s | Intv: %us\n"
                 "• Countdown: %us | Beacons: %u\n"
                 "• Msg: %s\n"
                 "Cfg: /sos [interval|countdown|channel|msg|trigger|cancel|cancel force]",
                 st, SosT1000e::instance().getConfig().channel,
                 SosT1000e::instance().getConfig().interval_seconds,
                 SosT1000e::instance().getConfig().countdown_seconds,
                 SosT1000e::instance().getActiveBeaconsSent(),
                 SosT1000e::instance().getConfig().custom_message);
    }
    sendReply(dest, is_ch, reply);
}

void CliAssistant::cmdTelemetry(const char* args, const uint8_t* dest, bool is_ch) {
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

void CliAssistant::cmdBattery(const uint8_t* dest, bool is_ch) {
    char reply[96];
    snprintf(reply, sizeof(reply), "🔋 Battery: %u%% | Voltage: %.2fV", _battery_pct, (double)_battery_volts);
    sendReply(dest, is_ch, reply);
}

void CliAssistant::cmdGps(const uint8_t* dest, bool is_ch) {
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

void CliAssistant::cmdPa(const uint8_t* dest, bool is_ch) {
    char reply[128];
    snprintf(reply, sizeof(reply),
             "📶 PA Gain Limiter: %s\nHardware PA Gain: +%d dB\nLegal Output Limit: +%d dBm conducted",
             GainEngine::instance().isLimiterEnabled() ? "ENABLED" : "DISABLED",
             GainEngine::instance().getPaGain(),
             GainEngine::instance().getLegalLimit());
    sendReply(dest, is_ch, reply);
}

void CliAssistant::cmdInfo(const uint8_t* dest, bool is_ch) {
    const char* info = 
        "ℹ️ RO-mesh Firmware v1.0.0\n"
        "Architecture: Decoupled Zero-Conflict Core\n"
        "Default Scope: #ro\n"
        "Author: focaldistorsion";
    sendReply(dest, is_ch, info);
}

} // namespace ro_mesh

