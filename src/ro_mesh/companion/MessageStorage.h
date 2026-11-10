#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

namespace ro_mesh {

struct StoredMessage {
    uint32_t timestamp;
    uint8_t sender_id[4];     // 4-byte prefix of sender public key
    char sender_name[16];
    char channel_name[16];    // Empty if direct message
    char text[128];
    bool is_outgoing;
    bool is_unread;
};

class MessageStorage {
public:
    static constexpr size_t MAX_STORED_MESSAGES = 32;

    static MessageStorage& instance();

    void init();
    bool addMessage(const uint8_t* sender_pubkey, const char* sender_name,
                    const char* channel_name, const char* text,
                    bool is_outgoing, uint32_t timestamp = 0);

    size_t getCount() const { return _count; }
    size_t getUnreadCount() const;
    void markAllRead();

    const StoredMessage* getMessage(size_t index) const;
    void clear();

private:
    MessageStorage();

    StoredMessage _messages[MAX_STORED_MESSAGES];
    size_t _head;
    size_t _count;
};

} // namespace ro_mesh

