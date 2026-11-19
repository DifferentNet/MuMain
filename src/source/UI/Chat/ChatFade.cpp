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

int SystemLinesShown(const std::vector<std::uint64_t>& arrivalsMs, std::uint64_t nowMs, int maxLines,
                     std::uint64_t stayMs, bool chatOpen)
{
    int count = 0;
    for (auto it = arrivalsMs.rbegin(); it != arrivalsMs.rend() && count < maxLines; ++it)
    {
        if (!chatOpen && nowMs > *it && nowMs - *it >= stayMs)
            break;
        ++count;
    }
    return count;
}

bool FoldsIntoNewestSystemLine(bool sameAsNewest, std::uint64_t newestArrivalMs, std::uint64_t nowMs)
{
    if (!sameAsNewest)
        return false;
    // A clock that went back (or a stamp from the same tick) still counts as a repeat.
    return nowMs <= newestArrivalMs || nowMs - newestArrivalMs < SystemLineRepeatMs;
}

std::wstring WithRepeatCount(const std::wstring& text, int count)
{
    if (count <= 1)
        return text;
    return text + L" \u00D7" + std::to_wstring(count);
}
} // namespace UI::Chat
