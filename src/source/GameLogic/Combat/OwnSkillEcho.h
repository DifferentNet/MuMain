#pragma once

#include <array>
#include <chrono>

namespace GameLogic::Combat
{
// The server answers every area skill the hero casts with the animation packet it sends to everyone around
// (ReceiveMagicContinue). For the hero's own volley skills the client starts the swing's timer when casting, so
// the answer must not start it a second time: it arrives a round trip later, and on a busy connection it lands in
// a later swing or together with other answers. Started by the answer alone, some swings fired no arrows on the
// player's own screen and others fired twice, while everyone else saw every volley.
class OwnSkillEcho
{
public:
    // A cast whose answer is still on its way.
    static void Expect(int skill)
    {
        Entry& entry = Entries()[Next()];
        entry.skill = skill;
        entry.sent = Clock::now();
        Next() = (Next() + 1) % Capacity;
    }

    // True when the answer is to a cast the hero already played; that cast is then no longer expected.
    static bool Consume(int skill)
    {
        const auto now = Clock::now();
        Entry* oldest = nullptr;
        for (Entry& entry : Entries())
        {
            if (entry.skill != skill || now - entry.sent > Lifetime)
            {
                continue;
            }
            if (oldest == nullptr || entry.sent < oldest->sent)
            {
                oldest = &entry;
            }
        }
        if (oldest == nullptr)
        {
            return false;
        }
        oldest->skill = -1;
        return true;
    }

private:
    using Clock = std::chrono::steady_clock;

    struct Entry
    {
        int skill = -1;
        Clock::time_point sent{};
    };

    static constexpr std::size_t Capacity = 8;
    // A cast the server refused gets no answer; after this long it is no longer waited for.
    static constexpr auto Lifetime = std::chrono::seconds(3);

    static std::array<Entry, Capacity>& Entries()
    {
        static std::array<Entry, Capacity> entries;
        return entries;
    }

    static std::size_t& Next()
    {
        static std::size_t next = 0;
        return next;
    }
};
} // namespace GameLogic::Combat
