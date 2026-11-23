#include "ChatManager.h"

namespace RANMobile {

void ChatManager::AddMessage(ChatChannel channel, const std::string& sender, const std::string& textUtf8) {
    if (m_messages.size() >= m_maxMessages) {
        m_messages.erase(m_messages.begin());
    }
    m_messages.push_back({channel, sender, textUtf8});
}

} // namespace RANMobile
