#include "AutomaticRegions.h"
#include <string.h>
#include <stdio.h>

namespace ro_mesh {

SystemNotificationCallback AutomaticRegions::_notify_cb = nullptr;

AutomaticRegions::AutomaticRegions() : _initialized(false) {
  strncpy(_active_scope, "#ro", sizeof(_active_scope) - 1);
  _active_scope[sizeof(_active_scope) - 1] = '\0';
}

void AutomaticRegions::init(float lat, float lon, const char* node_name) {
  const CountyEntry* county = nullptr;

  if (lat != 0.0f || lon != 0.0f) {
    county = findClosestCounty(lat, lon);
  }

  if (!county && node_name) {
    county = findCountyFromNodeName(node_name);
  }

  if (county) {
    strncpy(_active_scope, county->region, sizeof(_active_scope) - 1);
  } else {
    strncpy(_active_scope, "#ro", sizeof(_active_scope) - 1);
    if (_notify_cb) {
      _notify_cb("Notice: GPS coordinates unset and node name non-conforming. Defaulting to #ro scope.");
    }
  }
  _active_scope[sizeof(_active_scope) - 1] = '\0';
  _initialized = true;
}

bool AutomaticRegions::evaluateAndInject(RegionMap& region_map, const char* node_name, float lat, float lon) {
  const CountyEntry* county = nullptr;

  // 1. Try resolving via GPS coordinates if valid
  if (lat != 0.0f || lon != 0.0f) {
    county = findClosestCounty(lat, lon);
  }

  // 2. Fallback to node-name prefix lookup if no GPS coordinates
  if (!county && node_name) {
    county = findCountyFromNodeName(node_name);
  }

  bool changed = false;

  // Step 1: Ensure #europe root exists
  auto r_eu = region_map.findByName("#europe");
  if (!r_eu) {
    r_eu = region_map.putRegion("#europe", 0);
    changed = true;
  }
  uint16_t id_eu = r_eu ? r_eu->id : 0;

  // Step 2: Ensure #ro country exists under #europe
  auto r_ro = region_map.findByName("#ro");
  if (!r_ro) {
    r_ro = region_map.putRegion("#ro", id_eu);
    changed = true;
  }
  uint16_t id_ro = r_ro ? r_ro->id : 0;

  // Step 3: Ensure RF sub-band #ro-868 exists under #ro
  auto r_868 = region_map.findByName("#ro-868");
  if (!r_868) {
    r_868 = region_map.putRegion("#ro-868", id_ro);
    changed = true;
  }

  // If a specific county was determined
  if (county) {
    // Ensure Macro-Region exists under #ro
    auto r_macro = region_map.findByName(county->macro);
    if (!r_macro) {
      r_macro = region_map.putRegion(county->macro, id_ro);
      changed = true;
    }
    uint16_t id_macro = r_macro ? r_macro->id : id_ro;

    // Ensure County Region exists under Macro-Region
    auto r_county = region_map.findByName(county->region);
    if (!r_county) {
      r_county = region_map.putRegion(county->region, id_macro);
      changed = true;
    }

    // Set default scope to this county
    region_map.setDefaultRegion(r_county);
    region_map.setHomeRegion(r_county);
  } else {
    // Fallback: Default to national #ro scope
    auto r_ro = region_map.findByName("#ro");
    region_map.setDefaultRegion(r_ro);
    region_map.setHomeRegion(r_ro);

    if (_notify_cb) {
      _notify_cb("Notice: GPS coordinates unset and node name non-conforming. Defaulting to #ro scope.");
    }
  }

  return changed;
}

} // namespace ro_mesh
