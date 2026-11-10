#pragma once

#include <Arduino.h>
#include <helpers/RegionMap.h>
#include "RomanianRegions.h"

namespace ro_mesh {

typedef void (*SystemNotificationCallback)(const char* message);

class AutomaticRegions {
  static SystemNotificationCallback _notify_cb;
  char _active_scope[32];
  bool _initialized;

  AutomaticRegions();

public:
  static AutomaticRegions& instance() {
    static AutomaticRegions inst;
    return inst;
  }

  void init(float lat = 0.0f, float lon = 0.0f, const char* node_name = nullptr);

  const char* getActiveScope() const { return _active_scope; }

  static void setNotificationCallback(SystemNotificationCallback cb) {
    _notify_cb = cb;
  }

  /**
   * Evaluates node location or name, injecting the Romanian region hierarchy into RegionMap.
   * Strictly active on repeaters.
   *
   * @param region_map The MeshCore RegionMap instance.
   * @param node_name The node's configured name from preferences.
   * @param lat Configured latitude (or 0.0 if unset).
   * @param lon Configured longitude (or 0.0 if unset).
   * @return true if region hierarchy was successfully injected/updated.
   */
  static bool evaluateAndInject(RegionMap& region_map, const char* node_name, float lat = 0.0f, float lon = 0.0f);
};

} // namespace ro_mesh
