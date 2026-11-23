#pragma once

#include <string>
#include <vector>

namespace RANMobile {

enum class ChatChannel {
    Normal,
    Whisper,
    Party,
    Guild,
    System
};

struct ChatMessage {
    ChatChannel channel = ChatChannel::Normal;
    std::string sender;
    std::string textUtf8;
};

class ChatManager {
public:
    void AddMessage(ChatChannel channel, const std::string& sender, const std::string& textUtf8);
    const std::vector<ChatMessage>& GetMessages() const { return m_messages; }
    void Clear() { m_messages.clear(); }

private:
    std::vector<ChatMessage> m_messages;
    size_t m_maxMessages = 200;
};

} // namespace RANMobile
