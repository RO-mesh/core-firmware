#pragma once

#include <Arduino.h>
#include <DataStore.h>
#include <helpers/TxtDataHelpers.h>

namespace ro_mesh {

// Official MeshCore Global Public Key (16 bytes)
static const uint8_t MESHCORE_PUBLIC_PSK[16] = {
  0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
  0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72
};

// Fixed deterministic PSKs derived from SHA256(hashtag).slice(0, 16)
// Matching /v2/configurator/js/romesh_config.js standard
// SHA256("#romania") -> 88b0cbcf73f4b8cf6c40a5a3a0eec38d
static const uint8_t ROMESH_ROMANIA_PSK[16] = {
  0x88, 0xb0, 0xcb, 0xcf, 0x73, 0xf4, 0xb8, 0xcf,
  0x6c, 0x40, 0xa5, 0xa3, 0xa0, 0xee, 0xc3, 0x8d
};

// SHA256("#sos") -> 95e869cecfad88bf3a0a6566cbdf6fa6
static const uint8_t ROMESH_SOS_PSK[16] = {
  0x95, 0xe8, 0x69, 0xce, 0xcf, 0xad, 0x88, 0xbf,
  0x3a, 0x0a, 0x65, 0x66, 0xcb, 0xdf, 0x6f, 0xa6
};

// SHA256("#alerte") -> d9b2229fa1cfcb5c8ce6ee656755490d
static const uint8_t ROMESH_ALERTE_PSK[16] = {
  0xd9, 0xb2, 0x22, 0x9f, 0xa1, 0xcf, 0xcb, 0x5c,
  0x8c, 0xe6, 0xee, 0x65, 0x67, 0x55, 0x49, 0x0d
};

// Dedicated in-chat CLI control channel PSK
// SHA256("RO-mesh-T1000E-CLI-v1") -> 4f8a31e8bb921c5f60d48be6ce28ad5e
static const uint8_t ROMESH_CLI_PSK[16] = {
  0x4f, 0x8a, 0x31, 0xe8, 0xbb, 0x92, 0x1c, 0x5f,
  0x60, 0xd4, 0x8b, 0xe6, 0xce, 0x28, 0xad, 0x5e
};

/**
 * Pre-populates the standard Romanian and system channels into DataStore if not present.
 * Ensures an instant zero-setup onboarding experience when connecting via the companion app.
 */
inline void initDefaultChannels(DataStore& store, bool isT1000e = false) {
  // Check if Public channel is already provisioned
  ChannelDetails ch;
  bool hasPublic = false;
  bool hasRomania = false;
  bool hasSos = false;
  bool hasAlerte = false;
  bool hasCli = false;

  for (int i = 0; i < store.getNumChannels(); i++) {
    if (store.getChannel(i, ch)) {
      if (strcmp(ch.channel_name, "Public") == 0) hasPublic = true;
      if (strcmp(ch.channel_name, "#romania") == 0) hasRomania = true;
      if (strcmp(ch.channel_name, "#sos") == 0) hasSos = true;
      if (strcmp(ch.channel_name, "#alerte") == 0) hasAlerte = true;
      if (strcmp(ch.channel_name, "RO-mesh T1000E CLI") == 0 || strcmp(ch.channel_name, "System") == 0) hasCli = true;
    }
  }

  // 1. Official Global Public Channel
  if (!hasPublic) {
    memset(&ch, 0, sizeof(ch));
    strncpy(ch.channel_name, "Public", sizeof(ch.channel_name) - 1);
    memcpy(ch.channel_secret, MESHCORE_PUBLIC_PSK, 16);
    store.saveChannel(store.getNumChannels(), ch);
  }

  // 2. #romania Channel
  if (!hasRomania) {
    memset(&ch, 0, sizeof(ch));
    strncpy(ch.channel_name, "#romania", sizeof(ch.channel_name) - 1);
    memcpy(ch.channel_secret, ROMESH_ROMANIA_PSK, 16);
    store.saveChannel(store.getNumChannels(), ch);
  }

  // 3. #sos Emergency Channel
  if (!hasSos) {
    memset(&ch, 0, sizeof(ch));
    strncpy(ch.channel_name, "#sos", sizeof(ch.channel_name) - 1);
    memcpy(ch.channel_secret, ROMESH_SOS_PSK, 16);
    store.saveChannel(store.getNumChannels(), ch);
  }

  // 4. #alerte Alert Channel
  if (!hasAlerte) {
    memset(&ch, 0, sizeof(ch));
    strncpy(ch.channel_name, "#alerte", sizeof(ch.channel_name) - 1);
    memcpy(ch.channel_secret, ROMESH_ALERTE_PSK, 16);
    store.saveChannel(store.getNumChannels(), ch);
  }

  // 5. In-Chat CLI Control Channel (strictly for T1000-E companion builds)
  if (isT1000e && !hasCli) {
    memset(&ch, 0, sizeof(ch));
    strncpy(ch.channel_name, "RO-mesh T1000E CLI", sizeof(ch.channel_name) - 1);
    memcpy(ch.channel_secret, ROMESH_CLI_PSK, 16);
    store.saveChannel(store.getNumChannels(), ch);
  }
}

} // namespace ro_mesh

