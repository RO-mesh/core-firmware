#pragma once
#include <stdint.h>
#include <string.h>

struct ChannelDetails {
  char channel_name[32];
  uint8_t channel_secret[16];
};

class DataStore {
public:
  int getNumChannels() const { return 0; }
  bool getChannel(int, ChannelDetails&) { return false; }
  void saveChannel(int, const ChannelDetails&) {}
};
