#pragma once

#include <Arduino.h>

namespace ro_mesh {

struct MacroRegion {
  const char* id;
  const char* name;
  const char* parent;
};

static const MacroRegion ROMESH_MACRO_REGIONS[] = {
  { "ro-nord-vest",        "Nord-Vest",          "ro" },
  { "ro-centru",           "Centru",             "ro" },
  { "ro-nord-est",         "Nord-Est",           "ro" },
  { "ro-sud-est",          "Sud-Est",            "ro" },
  { "ro-sud-muntenia",     "Sud-Muntenia",       "ro" },
  { "ro-b-if",             "Bucuresti-Ilfov",    "ro" },
  { "ro-sud-vest-oltenia", "Sud-Vest-Oltenia",   "ro" },
  { "ro-vest",             "Vest",               "ro" }
};
static const size_t NUM_MACRO_REGIONS = sizeof(ROMESH_MACRO_REGIONS) / sizeof(ROMESH_MACRO_REGIONS[0]);

struct CountyEntry {
  const char* code;     // e.g. "BV"
  const char* name;     // e.g. "Brasov"
  const char* region;   // e.g. "ro-bv"
  const char* macro;    // e.g. "ro-centru"
  float lat;
  float lon;
};

static const CountyEntry ROMESH_COUNTIES[] = {
  // Nord-Vest (6)
  { "BH", "Bihor",           "ro-bh", "ro-nord-vest", 47.0722f, 21.9211f },
  { "BN", "Bistrita-Nasaud", "ro-bn", "ro-nord-vest", 47.1333f, 24.5000f },
  { "CJ", "Cluj",            "ro-cj", "ro-nord-vest", 46.7712f, 23.6236f },
  { "MM", "Maramures",       "ro-mm", "ro-nord-vest", 47.6597f, 23.5795f },
  { "SM", "Satu Mare",       "ro-sm", "ro-nord-vest", 47.7900f, 22.8900f },
  { "SJ", "Salaj",           "ro-sj", "ro-nord-vest", 47.1833f, 23.0500f },

  // Centru (6)
  { "AB", "Alba",            "ro-ab", "ro-centru",    46.0667f, 23.5833f },
  { "BV", "Brasov",          "ro-bv", "ro-centru",    45.6579f, 25.6012f },
  { "CV", "Covasna",         "ro-cv", "ro-centru",    45.8636f, 26.1389f },
  { "HR", "Harghita",        "ro-hr", "ro-centru",    46.3600f, 25.8000f },
  { "MS", "Mures",           "ro-ms", "ro-centru",    46.5456f, 24.5625f },
  { "SB", "Sibiu",           "ro-sb", "ro-centru",    45.7983f, 24.1256f },

  // Nord-Est (6)
  { "BC", "Bacau",           "ro-bc", "ro-nord-est",  46.5670f, 26.9140f },
  { "BT", "Botosani",        "ro-bt", "ro-nord-est",  47.7460f, 26.6690f },
  { "IS", "Iasi",            "ro-is", "ro-nord-est",  47.1585f, 27.6014f },
  { "NT", "Neamt",           "ro-nt", "ro-nord-est",  46.9383f, 26.3797f },
  { "SV", "Suceava",         "ro-sv", "ro-nord-est",  47.6514f, 26.2556f },
  { "VS", "Vaslui",          "ro-vs", "ro-nord-est",  46.6407f, 27.7276f },

  // Sud-Est (6)
  { "BR", "Braila",          "ro-br", "ro-sud-est",   45.2692f, 27.9575f },
  { "BZ", "Buzau",           "ro-bz", "ro-sud-est",   45.1500f, 26.8167f },
  { "CT", "Constanta",       "ro-ct", "ro-sud-est",   44.1807f, 28.6343f },
  { "GL", "Galati",          "ro-gl", "ro-sud-est",   45.4353f, 28.0080f },
  { "TL", "Tulcea",          "ro-tl", "ro-sud-est",   45.1797f, 28.8045f },
  { "VN", "Vrancea",         "ro-vn", "ro-sud-est",   45.6960f, 27.1830f },

  // Sud - Muntenia (7)
  { "AG", "Arges",           "ro-ag", "ro-sud-muntenia", 44.8565f, 24.8692f },
  { "CL", "Calarasi",        "ro-cl", "ro-sud-muntenia", 44.2000f, 27.3333f },
  { "DB", "Dambovita",       "ro-db", "ro-sud-muntenia", 44.9250f, 25.4570f },
  { "GR", "Giurgiu",         "ro-gr", "ro-sud-muntenia", 43.9037f, 25.9699f },
  { "IL", "Ialomita",        "ro-il", "ro-sud-muntenia", 44.5667f, 27.3667f },
  { "PH", "Prahova",         "ro-ph", "ro-sud-muntenia", 44.9392f, 26.0375f },
  { "TR", "Teleorman",       "ro-tr", "ro-sud-muntenia", 43.9750f, 25.3333f },

  // Bucuresti - Ilfov (2)
  { "B",  "Bucuresti",       "ro-b",  "ro-b-if",      44.4323f, 26.1063f },
  { "IF", "Ilfov",           "ro-if", "ro-b-if",      44.5385f, 26.1293f },

  // Sud-Vest Oltenia (5)
  { "DJ", "Dolj",            "ro-dj", "ro-sud-vest-oltenia", 44.3302f, 23.7949f },
  { "GJ", "Gorj",            "ro-gj", "ro-sud-vest-oltenia", 45.0333f, 23.2667f },
  { "MH", "Mehedinti",       "ro-mh", "ro-sud-vest-oltenia", 44.6319f, 22.6561f },
  { "OT", "Olt",             "ro-ot", "ro-sud-vest-oltenia", 44.4300f, 24.3600f },
  { "VL", "Valcea",          "ro-vl", "ro-sud-vest-oltenia", 45.1047f, 24.3693f },

  // Vest (4)
  { "AR", "Arad",            "ro-ar", "ro-vest",      46.1866f, 21.3123f },
  { "CS", "Caras-Severin",   "ro-cs", "ro-vest",      45.3000f, 21.8890f },
  { "HD", "Hunedoara",       "ro-hd", "ro-vest",      45.8833f, 22.9000f },
  { "TM", "Timis",           "ro-tm", "ro-vest",      45.7489f, 21.2087f }
};
static const size_t NUM_COUNTIES = sizeof(ROMESH_COUNTIES) / sizeof(ROMESH_COUNTIES[0]);

/**
 * Finds county by 2-letter uppercase code (e.g. "BV", "CJ", "B")
 */
inline const CountyEntry* findCountyByCode(const char* code) {
  if (!code || !code[0]) return nullptr;
  for (size_t i = 0; i < NUM_COUNTIES; i++) {
    if (strcasecmp(ROMESH_COUNTIES[i].code, code) == 0) {
      return &ROMESH_COUNTIES[i];
    }
  }
  return nullptr;
}

/**
 * Finds county by exact region string (e.g. "ro-bv")
 */
inline const CountyEntry* findCountyByRegion(const char* region) {
  if (!region || !region[0]) return nullptr;
  for (size_t i = 0; i < NUM_COUNTIES; i++) {
    if (strcasecmp(ROMESH_COUNTIES[i].region, region) == 0) {
      return &ROMESH_COUNTIES[i];
    }
  }
  return nullptr;
}

/**
 * Parses node name looking for RO-<COUNTY>-<ZONE> or <COUNTY>-<ZONE> pattern.
 * Examples:
 *   "RO-BV-POSTAVARU" -> matches "BV"
 *   "RO-CJ-FELEACU"   -> matches "CJ"
 *   "RO-B-HERASTRAU"  -> matches "B"
 *   "BV-RELAY"        -> matches "BV"
 */
inline const CountyEntry* findCountyFromNodeName(const char* node_name) {
  if (!node_name || !node_name[0]) return nullptr;

  const char* p = node_name;
  if (strncasecmp(p, "RO-", 3) == 0) {
    p += 3;
  }

  char token[8] = {0};
  size_t idx = 0;
  while (*p && *p != '-' && *p != '_' && *p != ' ' && idx < sizeof(token) - 1) {
    token[idx++] = *p++;
  }
  token[idx] = '\0';

  return findCountyByCode(token);
}

/**
 * Finds closest Romanian county based on GPS coordinates.
 * Uses Euclidean approximation calibrated for Romanian latitude (~45 deg N).
 */
inline const CountyEntry* findClosestCounty(float lat, float lon) {
  if (lat == 0.0f && lon == 0.0f) return nullptr;

  const CountyEntry* best = nullptr;
  float best_dist_sq = 1e9f;

  for (size_t i = 0; i < NUM_COUNTIES; i++) {
    float dlat = lat - ROMESH_COUNTIES[i].lat;
    float dlon = (lon - ROMESH_COUNTIES[i].lon) * 0.7071f; // cos(45 deg)
    float dist_sq = dlat * dlat + dlon * dlon;
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best = &ROMESH_COUNTIES[i];
    }
  }

  return best;
}

} // namespace ro_mesh
