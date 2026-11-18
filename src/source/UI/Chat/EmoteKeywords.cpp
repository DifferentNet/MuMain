#include "UI/Chat/EmoteKeywords.h"

#include <algorithm>

namespace UI::Chat::EmoteKeywords
{
namespace
{
constexpr wchar_t AsciiUpperFirst = L'A';
constexpr wchar_t AsciiUpperLast = L'Z';
constexpr wchar_t AsciiCaseOffset = L'a' - L'A';
// Latin-1: À..Þ, without the multiplication sign.
constexpr wchar_t Latin1UpperFirst = 0x00C0;
constexpr wchar_t Latin1UpperLast = 0x00DE;
constexpr wchar_t MultiplicationSign = 0x00D7;
constexpr wchar_t DivisionSign = 0x00F7;
// Latin Extended-A pairs upper/lower case as neighbours: even upper in two runs, odd upper in two others.
constexpr wchar_t LatinExtAFirst = 0x0100;
constexpr wchar_t LatinExtAEvenRun1Last = 0x0137;
constexpr wchar_t LatinExtAOddRun1First = 0x0139;
constexpr wchar_t LatinExtAOddRun1Last = 0x0148;
constexpr wchar_t LatinExtAEvenRun2First = 0x014A;
constexpr wchar_t LatinExtAEvenRun2Last = 0x0177;
constexpr wchar_t LatinCapitalYDiaeresis = 0x0178;
constexpr wchar_t LatinSmallYDiaeresis = 0x00FF;
constexpr wchar_t LatinExtAOddRun2First = 0x0179;
constexpr wchar_t LatinExtAOddRun2Last = 0x017E;
constexpr wchar_t LatinExtBLast = 0x024F;
// Cyrillic: Ѐ..Џ (+0x50), А..Я (+0x20), and the paired letters from Ѡ on (even upper), as Ґ/ґ.
constexpr wchar_t CyrillicFirst = 0x0400;
constexpr wchar_t CyrillicExtraUpperLast = 0x040F;
constexpr wchar_t CyrillicExtraOffset = 0x0050;
constexpr wchar_t CyrillicUpperFirst = 0x0410;
constexpr wchar_t CyrillicUpperLast = 0x042F;
constexpr wchar_t CyrillicCaseOffset = 0x0020;
constexpr wchar_t CyrillicPairsRun1First = 0x0460;
constexpr wchar_t CyrillicPairsRun1Last = 0x0481;
constexpr wchar_t CyrillicPairsRun2First = 0x048A;
constexpr wchar_t CyrillicPairsRun2Last = 0x04BF;
constexpr wchar_t CyrillicLast = 0x04FF;

bool InRange(wchar_t c, wchar_t first, wchar_t last)
{
    return c >= first && c <= last;
}

bool IsEven(wchar_t c)
{
    return (c & 1) == 0;
}

wchar_t FoldLatin(wchar_t c)
{
    if (InRange(c, AsciiUpperFirst, AsciiUpperLast))
        return static_cast<wchar_t>(c + AsciiCaseOffset);
    if (InRange(c, Latin1UpperFirst, Latin1UpperLast) && c != MultiplicationSign)
        return static_cast<wchar_t>(c + AsciiCaseOffset);
    const bool evenRun = InRange(c, LatinExtAFirst, LatinExtAEvenRun1Last) ||
                         InRange(c, LatinExtAEvenRun2First, LatinExtAEvenRun2Last);
    if (evenRun && IsEven(c))
        return static_cast<wchar_t>(c + 1);
    const bool oddRun = InRange(c, LatinExtAOddRun1First, LatinExtAOddRun1Last) ||
                        InRange(c, LatinExtAOddRun2First, LatinExtAOddRun2Last);
    if (oddRun && !IsEven(c))
        return static_cast<wchar_t>(c + 1);
    if (c == LatinCapitalYDiaeresis)
        return LatinSmallYDiaeresis;
    return c;
}

wchar_t FoldCyrillic(wchar_t c)
{
    if (InRange(c, CyrillicFirst, CyrillicExtraUpperLast))
        return static_cast<wchar_t>(c + CyrillicExtraOffset);
    if (InRange(c, CyrillicUpperFirst, CyrillicUpperLast))
        return static_cast<wchar_t>(c + CyrillicCaseOffset);
    const bool pairs = InRange(c, CyrillicPairsRun1First, CyrillicPairsRun1Last) ||
                       InRange(c, CyrillicPairsRun2First, CyrillicPairsRun2Last);
    if (pairs && IsEven(c))
        return static_cast<wchar_t>(c + 1);
    return c;
}

bool IsWholeWordKeyword(std::wstring_view keyword)
{
    return keyword.size() <= MaxWholeWordLength && std::all_of(keyword.begin(), keyword.end(), IsWordCharacter);
}

bool IsSeveralWords(std::wstring_view keyword)
{
    return keyword.find(L' ') != std::wstring_view::npos;
}

bool StartsWord(std::wstring_view text, std::size_t position)
{
    return position == 0 || !IsWordCharacter(text[position - 1]);
}

bool StandsAlone(std::wstring_view text, std::size_t position, std::size_t length)
{
    const std::size_t end = position + length;
    const bool endsWord = end == text.size() || !IsWordCharacter(text[end]);
    return StartsWord(text, position) && endsWord;
}

// Whether the keyword is anywhere in the text, letter case as typed: how every keyword matched before.
bool ContainsExactly(std::wstring_view text, std::wstring_view keyword)
{
    return !keyword.empty() && text.find(keyword) != std::wstring_view::npos;
}
} // namespace

wchar_t FoldCase(wchar_t c)
{
    if (c < CyrillicFirst)
        return FoldLatin(c);
    return FoldCyrillic(c);
}

std::wstring FoldCase(std::wstring_view text)
{
    std::wstring folded(text);
    for (wchar_t& c : folded)
        c = FoldCase(c);
    return folded;
}

bool IsWordCharacter(wchar_t c)
{
    if (InRange(c, L'0', L'9') || InRange(c, L'a', L'z') || InRange(c, L'A', L'Z'))
        return true;
    if (InRange(c, Latin1UpperFirst, LatinExtBLast))
        return c != MultiplicationSign && c != DivisionSign;
    if (InRange(c, CyrillicFirst, CyrillicLast))
        return !InRange(c, CyrillicPairsRun1Last + 1, CyrillicPairsRun2First - 1);
    return false;
}

bool HasCyrillic(std::wstring_view keyword)
{
    return std::any_of(keyword.begin(), keyword.end(), [](wchar_t c) { return InRange(c, CyrillicFirst, CyrillicLast); });
}

bool ContainsKeyword(std::wstring_view foldedText, std::wstring_view foldedKeyword, bool anywhere)
{
    if (foldedKeyword.empty())
        return false;

    const bool wholeWord = !anywhere && IsWholeWordKeyword(foldedKeyword);
    const bool startsWord = !anywhere && IsSeveralWords(foldedKeyword);
    for (std::size_t position = foldedText.find(foldedKeyword); position != std::wstring_view::npos;
         position = foldedText.find(foldedKeyword, position + 1))
    {
        if (wholeWord && !StandsAlone(foldedText, position, foldedKeyword.size()))
            continue;
        if (startsWord && !StartsWord(foldedText, position))
            continue;
        return true;
    }
    return false;
}

int FirstMatch(std::wstring_view text, std::span<const Group> groups)
{
    const std::wstring foldedText = FoldCase(text);
    for (std::size_t index = 0; index < groups.size(); ++index)
    {
        const Group& group = groups[index];
        for (const wchar_t* keyword : group.keywords)
        {
            if (keyword == nullptr)
                continue;
            const bool found = HasCyrillic(keyword) ? ContainsKeyword(foldedText, FoldCase(keyword), group.anywhere)
                                                    : ContainsExactly(text, keyword);
            if (found)
                return static_cast<int>(index);
        }
    }
    return -1;
}
} // namespace UI::Chat::EmoteKeywords
