#include "UI/Chat/ItemLinkTokens.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace UI::Chat::ItemLinkTokens
{
namespace
{
constexpr std::uint32_t FirstHighSurrogate = 0xD800;
constexpr std::uint32_t FirstLowSurrogate = 0xDC00;
constexpr std::uint32_t PastLowSurrogates = 0xE000;

// One edit: `before`'s [removedStart, removedEnd) became `inserted` characters.
struct Change
{
    std::size_t removedStart = 0;
    std::size_t removedEnd = 0;
    std::size_t inserted = 0;
};

// Typing, pasting, Backspace and Delete leave the caret where the edit ends: what follows it is `before`'s tail.
std::optional<Change> ChangeAtCaret(std::wstring_view before, std::wstring_view after, std::size_t caretAfter)
{
    if (caretAfter > after.size())
        return std::nullopt;
    const std::size_t tail = after.size() - caretAfter;
    if (tail > before.size() || before.substr(before.size() - tail) != after.substr(caretAfter))
        return std::nullopt;
    const std::size_t headLimit = std::min(before.size() - tail, caretAfter);
    std::size_t head = 0;
    while (head < headLimit && before[head] == after[head])
        ++head;
    return Change{head, before.size() - tail, caretAfter - head};
}

// Without a caret to go by: the narrowest edit that explains the two texts.
Change WidestCommonEnds(std::wstring_view before, std::wstring_view after)
{
    const std::size_t shortest = std::min(before.size(), after.size());
    std::size_t head = 0;
    while (head < shortest && before[head] == after[head])
        ++head;
    std::size_t tail = 0;
    while (tail < shortest - head && before[before.size() - 1 - tail] == after[after.size() - 1 - tail])
        ++tail;
    return Change{head, before.size() - tail, after.size() - tail - head};
}

bool IsEdgeDeletion(const Change& change, const Token& token)
{
    if (change.inserted != 0 || change.removedEnd - change.removedStart != 1)
        return false;
    return change.removedStart == token.start || change.removedStart == token.start + token.length - 1;
}

bool IsHighSurrogate(wchar_t c)
{
    const auto unit = static_cast<std::uint32_t>(c);
    return unit >= FirstHighSurrogate && unit < FirstLowSurrogate;
}

bool IsLowSurrogate(wchar_t c)
{
    const auto unit = static_cast<std::uint32_t>(c);
    return unit >= FirstLowSurrogate && unit < PastLowSurrogates;
}

// How many units the character at `index` takes.
std::size_t UnitsAt(std::wstring_view text, std::size_t index)
{
    if constexpr (sizeof(wchar_t) == 2)
    {
        if (IsHighSurrogate(text[index]) && index + 1 < text.size() && IsLowSurrogate(text[index + 1]))
            return 2;
    }
    return 1;
}
} // namespace

std::optional<Fix> ApplyEdit(std::wstring_view before, std::wstring_view after, std::size_t caretAfter,
                             std::vector<Token>& tokens)
{
    if (before == after || tokens.empty())
        return std::nullopt;

    const Change change = ChangeAtCaret(before, after, caretAfter).value_or(WidestCommonEnds(before, after));
    const auto delta = static_cast<std::ptrdiff_t>(change.inserted)
                       - static_cast<std::ptrdiff_t>(change.removedEnd - change.removedStart);
    std::optional<Fix> fix;
    std::ptrdiff_t takenOut = 0;
    std::vector<Token> kept;
    kept.reserve(tokens.size());
    for (const Token& token : tokens)
    {
        const std::size_t end = token.start + token.length;
        if (change.removedEnd <= token.start)
        {
            Token moved = token;
            moved.start = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(token.start) + delta + takenOut);
            kept.push_back(moved);
            continue;
        }
        if (change.removedStart >= end)
        {
            kept.push_back(token);
            continue;
        }
        // Touched: out whole for one character deleted at its edge, else it stays as plain text.
        if (!fix && IsEdgeDeletion(change, token))
        {
            std::wstring text(after);
            text.erase(token.start, token.length - 1);
            fix = Fix{std::move(text), token.start};
            takenOut = -static_cast<std::ptrdiff_t>(token.length - 1);
        }
    }
    tokens = std::move(kept);
    DropBroken(fix ? std::wstring_view(fix->text) : after, tokens);
    return fix;
}

void DropBroken(std::wstring_view text, std::vector<Token>& tokens)
{
    std::erase_if(tokens, [text](const Token& token)
                  {
                      const std::size_t end = token.start + token.length;
                      return token.length < 2 || end > text.size() || text[token.start] != L'['
                             || text[end - 1] != L']';
                  });
}

std::size_t InsertPoint(const std::vector<Token>& tokens, std::size_t caret)
{
    for (const Token& token : tokens)
    {
        if (caret > token.start && caret < token.start + token.length)
            return token.start + token.length;
    }
    return caret;
}

void AddToken(std::vector<Token>& tokens, std::size_t at, std::size_t length, int key)
{
    for (Token& token : tokens)
    {
        if (token.start >= at)
            token.start += length;
    }
    const auto place = std::find_if(tokens.begin(), tokens.end(), [at](const Token& token) { return token.start > at; });
    tokens.insert(place, Token{at, length, key});
}

std::size_t IndexOfCharacter(std::wstring_view text, std::size_t characters)
{
    std::size_t index = 0;
    for (std::size_t counted = 0; counted < characters && index < text.size(); ++counted)
        index += UnitsAt(text, index);
    return std::min(index, text.size());
}

std::size_t CharacterOfIndex(std::wstring_view text, std::size_t index)
{
    std::size_t characters = 0;
    for (std::size_t at = 0; at < index && at < text.size(); at += UnitsAt(text, at))
        ++characters;
    return characters;
}
} // namespace UI::Chat::ItemLinkTokens
