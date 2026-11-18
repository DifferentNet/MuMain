#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// With "Decline invitations" on (Options > Gameplay), party, trade and duel invitations are refused
// by themselves and each refusal is said in one chat line. A player who sends the same invitation again and
// again gets one line per QuietMs, so a spammer can't flood the chat with them.
namespace UI::Chat
{
enum class InviteKind
{
    Party,
    Trade,
    Duel,
};

class DeclinedInviteLines
{
public:
    static constexpr std::uint32_t QuietMs = 10'000;
    // Senders remembered at once; the oldest one is forgotten first.
    static constexpr std::size_t MaxRemembered = 16;

    // Whether the refusal of this invitation from this sender is said now.
    [[nodiscard]] bool ShouldSay(InviteKind kind, std::wstring_view sender, std::uint32_t nowMs);

    void Clear() { m_said.clear(); }

private:
    struct Said
    {
        InviteKind kind;
        std::wstring sender;
        std::uint32_t at;
    };
    std::vector<Said> m_said;
};
} // namespace UI::Chat
