#include <helpers/RegionMap.h>
#include <string.h>

static RegionEntry s_mock_regions[32];
static size_t s_mock_count = 0;
static const RegionEntry* s_default_region = nullptr;
static const RegionEntry* s_home_region = nullptr;

RegionEntry* RegionMap::findByName(const char* name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < s_mock_count; i++) {
        if (strcmp(s_mock_regions[i].name, name) == 0) {
            return &s_mock_regions[i];
        }
    }
    return nullptr;
}

RegionEntry* RegionMap::putRegion(const char* name, uint16_t parent_id, uint16_t id) {
    if (!name || s_mock_count >= 32) return nullptr;
    RegionEntry* r = &s_mock_regions[s_mock_count++];
    memset(r, 0, sizeof(RegionEntry));
    r->id = (id != 0) ? id : (uint16_t)s_mock_count;
    r->parent = parent_id;
    strncpy(r->name, name, sizeof(r->name) - 1);
    return r;
}

void RegionMap::setDefaultRegion(const RegionEntry* def) {
    s_default_region = def;
}

void RegionMap::setHomeRegion(const RegionEntry* home) {
    s_home_region = home;
}

