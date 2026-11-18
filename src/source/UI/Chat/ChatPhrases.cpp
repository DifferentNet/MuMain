#include "UI/Chat/ChatPhrases.h"

#include <array>
#include <cwchar>

namespace UI::Chat::Phrases
{
namespace
{
// The text every client can read, sent after the token. Part of the wire format: changing one only changes what
// old clients and logs show, the id carries the meaning.
constexpr std::array<std::wstring_view, Count> English = {
    L"Hello!",
    L"Thanks!",
    L"Invite me to the party",
    L"Wait for me",
    L"Help!",
    L"Let's go",
    L"Buff me, please",
    L"Selling, whisper me",
    L"Buying, whisper me",
    L"Sorry",
    L"Good game",
    L"Bye",
};

// "[#" two digits "]".
constexpr std::wstring_view TokenStart = L"[#";
constexpr wchar_t TokenEnd = L']';
constexpr std::size_t TokenLength = 5;
constexpr int DigitBase = 10;

bool InRange(int id)
{
    return id >= 1 && id <= Count;
}

bool IsDigit(wchar_t c)
{
    return c >= L'0' && c <= L'9';
}
} // namespace

std::wstring_view EnglishText(int id)
{
    if (!InRange(id))
        return {};
    return English[static_cast<std::size_t>(id - 1)];
}

std::wstring Compose(int id)
{
    if (!InRange(id))
        return {};
    wchar_t token[TokenLength + 1]{};
    std::swprintf(token, std::size(token), L"[#%02d]", id);
    std::wstring line(token);
    line += L' ';
    line += EnglishText(id);
    return line;
}

std::optional<int> IdOf(std::wstring_view text)
{
    if (text.size() < TokenLength || text.substr(0, TokenStart.size()) != TokenStart)
        return std::nullopt;
    const wchar_t tens = text[2];
    const wchar_t ones = text[3];
    if (!IsDigit(tens) || !IsDigit(ones) || text[4] != TokenEnd)
        return std::nullopt;
    if (text.size() > TokenLength && text[TokenLength] != L' ')
        return std::nullopt;
    const int id = (tens - L'0') * DigitBase + (ones - L'0');
    if (!InRange(id))
        return std::nullopt;
    return id;
}

std::optional<std::wstring> TryTranslate(std::wstring_view text, OwnTextOf ownText)
{
    const std::optional<int> id = IdOf(text);
    if (!id || ownText == nullptr)
        return std::nullopt;
    const wchar_t* own = ownText(*id);
    if (own == nullptr || own[0] == L'\0')
        return std::nullopt;
    return std::wstring(own);
}
} // namespace UI::Chat::Phrases
