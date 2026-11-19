#include "UI/Chat/InviterDetailText.h"

namespace UI::Chat::InviterDetail
{
namespace
{
constexpr std::wstring_view kSeparator = L" \u00B7 "; // a middle dot, spelled out for builds without /utf-8 (tests)
} // namespace

std::wstring Compose(std::wstring_view className, std::wstring_view guildWord, std::wstring_view guildName)
{
    if (className.empty())
        return {};

    std::wstring line(className);
    if (guildName.empty())
        return line;

    line.append(kSeparator);
    line.append(guildWord);
    line.push_back(L' ');
    line.append(guildName);
    return line;
}
} // namespace UI::Chat::InviterDetail
