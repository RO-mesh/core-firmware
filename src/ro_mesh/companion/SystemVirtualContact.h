#pragma once

#include <Arduino.h>
#include <helpers/ContactInfo.h>

namespace ro_mesh {

// Reserved 32-byte public key for the virtual local "System" contact
// [0x53, 0x59, 0x53, 0x54, 0x45, 0x4d, ...] ("SYSTEM" header + deterministic padding)
static const uint8_t SYSTEM_VIRTUAL_PUBKEY[PUB_KEY_SIZE] = {
  0x53, 0x59, 0x53, 0x54, 0x45, 0x4d, 0x00, 0x00, // "SYSTEM\0\0"
  0x52, 0x4f, 0x2d, 0x4d, 0x45, 0x53, 0x48, 0x00, // "RO-MESH\0"
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

/**
 * Checks if a given public key belongs to the virtual System contact.
 */
inline bool isSystemContact(const uint8_t* pub_key) {
  if (!pub_key) return false;
  return memcmp(pub_key, SYSTEM_VIRTUAL_PUBKEY, PUB_KEY_SIZE) == 0;
}

/**
 * Pre-populates the virtual "System" contact in the contact table if not already present.
 */
inline void initSystemVirtualContact(ContactInfo contacts[], int max_contacts, int& num_contacts) {
  for (int i = 0; i < num_contacts; i++) {
    if (isSystemContact(contacts[i].id.pub_key)) {
      return; // Already present
    }
  }

  if (num_contacts < max_contacts) {
    ContactInfo& sys = contacts[num_contacts++];
    memset(&sys, 0, sizeof(ContactInfo));
    memcpy(sys.id.pub_key, SYSTEM_VIRTUAL_PUBKEY, PUB_KEY_SIZE);
    strncpy(sys.name, "System", sizeof(sys.name) - 1);
    sys.type = 1; // Standard contact type
    sys.flags = 0x01 | 0x10; // Favorite + Remote CLI allowed
    sys.out_path_len = 0; // Local direct interface
    sys.lastmod = 1;
  }
}

} // namespace ro_mesh

