#pragma once

#include <string>
#include <string_view>

// The second line of a party, trade or duel invitation: who the inviter is beyond the name.
namespace UI::Chat::InviterDetail
{
// "Class <middle dot> Guild Name"; just the class when there is no guild; empty without a class.
[[nodiscard]] std::wstring Compose(std::wstring_view className, std::wstring_view guildWord,
                                   std::wstring_view guildName);
} // namespace UI::Chat::InviterDetail
