#pragma once

#include <cstdint>

// Enter on a chat line sooner than CooldownMs after the last line used to do nothing at all. Now the line is kept
// and goes by itself once the cooldown passed; Enter again while it waits changes nothing.
namespace UI::Chat
{
class ChatSendGate
{
public:
    enum class EnterResult
    {
        SendNow,
        Pending,
        AlreadyPending,
    };

    explicit ChatSendGate(std::uint64_t cooldownMs) : m_cooldownMs(cooldownMs) {}

    // Enter on a line at nowMs. SendNow counts the line as sent.
    [[nodiscard]] EnterResult OnEnter(std::uint64_t nowMs);
    // True once, when a waiting line may go at nowMs; it then counts as sent.
    [[nodiscard]] bool TakeDue(std::uint64_t nowMs);
    // The line is not sent after all (the bar closed).
    void Cancel() { m_pending = false; }
    [[nodiscard]] bool IsPending() const { return m_pending; }

private:
    [[nodiscard]] bool CooldownOver(std::uint64_t nowMs) const;

    std::uint64_t m_cooldownMs;
    std::uint64_t m_lastSentMs = 0;
    bool m_hasSent = false;
    bool m_pending = false;
};
} // namespace UI::Chat
