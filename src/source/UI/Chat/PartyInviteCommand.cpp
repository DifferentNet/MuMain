#include "UI/Chat/PartyInviteCommand.h"

#include <cwctype>

namespace UI::Chat
{
namespace
{
bool IsSpace(wchar_t c)
{
    return c == L' ' || c == L'\t';
}

std::wstring_view Trim(std::wstring_view text)
{
    while (!text.empty() && IsSpace(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && IsSpace(text.back()))
        text.remove_suffix(1);
    return text;
}
} // namespace

bool SameCharacterName(std::wstring_view a, std::wstring_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::towlower(static_cast<std::wint_t>(a[i])) != std::towlower(static_cast<std::wint_t>(b[i])))
            return false;
    }
    return true;
}

std::optional<std::wstring_view> CommandArgument(std::wstring_view text, std::initializer_list<std::wstring_view> commands,
                                                 std::wstring_view localizedCommand)
{
    text = Trim(text);
    std::size_t wordEnd = 0;
    while (wordEnd < text.size() && !IsSpace(text[wordEnd]))
        ++wordEnd;

    const std::wstring_view word = text.substr(0, wordEnd);
    bool isCommand = !localizedCommand.empty() && word == localizedCommand;
    for (const std::wstring_view command : commands)
        isCommand = isCommand || (!command.empty() && SameCharacterName(word, command));
    if (!isCommand)
        return std::nullopt;

    return Trim(text.substr(wordEnd));
}

std::optional<std::wstring_view> PartyCommandArgument(std::wstring_view text, std::wstring_view localizedCommand)
{
    return CommandArgument(text, { L"/party", L"/pt" }, localizedCommand);
}

std::optional<std::wstring_view> ReplyCommandArgument(std::wstring_view text)
{
    return CommandArgument(text, { L"/r", L"/reply" }, {});
}
} // namespace UI::Chat
