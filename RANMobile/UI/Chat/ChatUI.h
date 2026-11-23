#pragma once

#include "../../Client/Chat/ChatManager.h"
#include <string>

namespace RANMobile {

class ChatUI {
public:
    void Initialize(ChatManager* chatMgr) { m_chatManager = chatMgr; }
    void SetActiveChannel(ChatChannel channel) { m_activeChannel = channel; }
    ChatChannel GetActiveChannel() const { return m_activeChannel; }
    void SetInputTextUtf8(const std::string& text) { m_inputText = text; }
    const std::string& GetInputTextUtf8() const { return m_inputText; }
    void ClearInput() { m_inputText.clear(); }

private:
    ChatManager* m_chatManager = nullptr;
    ChatChannel m_activeChannel = ChatChannel::Normal;
    std::string m_inputText;
};

} // namespace RANMobile
