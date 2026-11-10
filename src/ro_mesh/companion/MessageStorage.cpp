#include "MessageStorage.h"
#include <string.h>

namespace ro_mesh {

MessageStorage& MessageStorage::instance() {
    static MessageStorage inst;
    return inst;
}

MessageStorage::MessageStorage() : _head(0), _count(0) {
    memset(_messages, 0, sizeof(_messages));
}

void MessageStorage::init() {
    _head = 0;
    _count = 0;
    memset(_messages, 0, sizeof(_messages));
}

bool MessageStorage::addMessage(const uint8_t* sender_pubkey, const char* sender_name,
                              const char* channel_name, const char* text,
                              bool is_outgoing, uint32_t timestamp) {
    if (!text || strlen(text) == 0) return false;

    StoredMessage& msg = _messages[_head];
    memset(&msg, 0, sizeof(StoredMessage));

    msg.timestamp = timestamp;
    if (sender_pubkey) {
        memcpy(msg.sender_id, sender_pubkey, 4);
    }
    if (sender_name) {
        strncpy(msg.sender_name, sender_name, sizeof(msg.sender_name) - 1);
    }
    if (channel_name) {
        strncpy(msg.channel_name, channel_name, sizeof(msg.channel_name) - 1);
    }
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.is_outgoing = is_outgoing;
    msg.is_unread = !is_outgoing;

    _head = (_head + 1) % MAX_STORED_MESSAGES;
    if (_count < MAX_STORED_MESSAGES) {
        _count++;
    }
    return true;
}

size_t MessageStorage::getUnreadCount() const {
    size_t unread = 0;
    for (size_t i = 0; i < _count; i++) {
        if (_messages[i].is_unread) {
            unread++;
        }
    }
    return unread;
}

void MessageStorage::markAllRead() {
    for (size_t i = 0; i < _count; i++) {
        _messages[i].is_unread = false;
    }
}

const StoredMessage* MessageStorage::getMessage(size_t index) const {
    if (index >= _count) return nullptr;
    // index 0 is oldest message
    size_t start_idx = (_head + MAX_STORED_MESSAGES - _count) % MAX_STORED_MESSAGES;
    size_t actual_idx = (start_idx + index) % MAX_STORED_MESSAGES;
    return &_messages[actual_idx];
}

void MessageStorage::clear() {
    _head = 0;
    _count = 0;
    memset(_messages, 0, sizeof(_messages));
}

} // namespace ro_mesh

