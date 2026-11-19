#pragma once

#include <string>

// The inviter's class and guild for an invitation dialog (InviterDetailText.h); empty when the player is not in
// sight, as the client then knows neither.
namespace UI::Chat::InviterDetail
{
[[nodiscard]] std::wstring ForKey(int characterKey);
[[nodiscard]] std::wstring ForName(const wchar_t* name);
} // namespace UI::Chat::InviterDetail
