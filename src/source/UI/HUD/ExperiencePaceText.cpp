#include "stdafx.h"
#include "UI/HUD/ExperiencePaceText.h"

#include "I18N/All.h"
#include "UI/HUD/ExperiencePace.h"

namespace UI::HUD::ExperiencePace
{
std::optional<std::wstring> TimeToGainText(std::int64_t remaining, double perHour)
{
    constexpr std::int64_t MinutesPerHour = 60;
    const auto minutes = MinutesToGain(remaining, perHour);
    if (!minutes || *minutes > MaxShownMinutes)
        return std::nullopt;

    wchar_t when[48] = {};
    if (*minutes >= MinutesPerHour)
        mu_swprintf(when, I18N::Game::DurationHoursMinutes, static_cast<int>(*minutes / MinutesPerHour),
                    static_cast<int>(*minutes % MinutesPerHour));
    else
        mu_swprintf(when, I18N::Game::DurationMinutes, static_cast<int>(*minutes));
    return std::wstring(when);
}
} // namespace UI::HUD::ExperiencePace
