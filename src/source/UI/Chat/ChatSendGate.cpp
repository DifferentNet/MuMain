#include "UI/Chat/ChatSendGate.h"

namespace UI::Chat
{
bool ChatSendGate::CooldownOver(std::uint64_t nowMs) const
{
    return !m_hasSent || nowMs - m_lastSentMs > m_cooldownMs;
}

ChatSendGate::EnterResult ChatSendGate::OnEnter(std::uint64_t nowMs)
{
    if (m_pending)
        return EnterResult::AlreadyPending;
    if (!CooldownOver(nowMs))
    {
        m_pending = true;
        return EnterResult::Pending;
    }
    m_lastSentMs = nowMs;
    m_hasSent = true;
    return EnterResult::SendNow;
}

bool ChatSendGate::TakeDue(std::uint64_t nowMs)
{
    if (!m_pending || !CooldownOver(nowMs))
        return false;
    m_pending = false;
    m_lastSentMs = nowMs;
    m_hasSent = true;
    return true;
}
} // namespace UI::Chat
