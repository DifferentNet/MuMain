#include "UI/Chat/ChatFade.h"

#include <algorithm>

namespace UI::Chat
{
bool IsChatLineShown(std::uint64_t addedAtMs, std::uint64_t nowMs, bool chatOpen)
{
    if (chatOpen || nowMs < addedAtMs)
        return true;
    return nowMs - addedAtMs < ChatLineShownMs;
}

ChatFadeState EvaluateChatFade(const std::vector<std::uint64_t>& addedAtMs, std::uint64_t nowMs, bool chatOpen)
{
    ChatFadeState state;
    state.shown.reserve(addedAtMs.size());
    for (std::size_t index = 0; index < addedAtMs.size(); ++index)
    {
        const bool shown = IsChatLineShown(addedAtMs[index], nowMs, chatOpen);
        state.shown.push_back(shown);
        if (!shown)
            continue;

        if (state.firstShown < 0)
            state.firstShown = static_cast<int>(index);
        if (chatOpen)
            continue;

        const std::uint64_t goesAtMs = addedAtMs[index] + ChatLineShownMs;
        state.nextChangeMs = state.nextChangeMs ? std::min(*state.nextChangeMs, goesAtMs) : goesAtMs;
    }
    return state;
}
} // namespace UI::Chat
