#pragma once

#include "UI/Chat/ItemLinkWire.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// MU Moldova, items shown in the chat (UI/Chat/ItemLinkWire.h): where the server's links stand in the chat line the
// client shows, and the pieces the chat log draws. The server counts in UTF-16 units; wchar_t is 16 bits on a PC and
// 32 bits on /play, so the text walks are written for both character sizes.
namespace UI::Chat::ItemLinkAttach
{
// A link's "[name]" in a text, in that text's own characters.
struct Span
{
    std::size_t start = 0;
    std::size_t length = 0;
};

// The index `units` UTF-16 units after `from`; empty past the end or inside a character.
template <class CharT>
std::optional<std::size_t> IndexAfterUtf16(std::basic_string_view<CharT> text, std::size_t from, std::size_t units)
{
    constexpr std::uint32_t FirstPairedCodePoint = 0x10000;
    if constexpr (sizeof(CharT) == 2)
    {
        if (from > text.size() || units > text.size() - from)
            return std::nullopt;
        return from + units;
    }
    else
    {
        std::size_t index = from;
        std::size_t counted = 0;
        while (counted < units)
        {
            if (index >= text.size())
                return std::nullopt;
            counted += static_cast<std::uint32_t>(text[index]) >= FirstPairedCodePoint ? 2 : 1;
            ++index;
        }
        if (counted != units)
            return std::nullopt;
        return index;
    }
}

// "[…]" of `units` UTF-16 units starting at index `start`, if the text has one there.
template <class CharT>
std::optional<Span> BracketsAt(std::basic_string_view<CharT> text, std::size_t start, std::size_t units)
{
    if (start >= text.size() || text[start] != CharT('['))
        return std::nullopt;
    const auto end = IndexAfterUtf16(text, start, units);
    if (!end || *end <= start || text[*end - 1] != CharT(']'))
        return std::nullopt;
    return Span{start, *end - start};
}

// The first "[…]" of `units` UTF-16 units at or after index `from`.
template <class CharT>
std::optional<Span> SearchBrackets(std::basic_string_view<CharT> text, std::size_t from, std::size_t units)
{
    for (std::size_t at = from; at < text.size(); ++at)
    {
        if (auto span = BracketsAt(text, at, units))
            return span;
    }
    return std::nullopt;
}

// Each link's place in the shown text, in the links' order: at its offset when "[" and "]" stand there, else the
// next "[…]" of its length after the link before it (a translated chat phrase moves the text), else none (the
// link stays plain text). Links are taken in the order of their offsets and never overlap.
template <class CharT>
std::vector<std::optional<Span>> Attach(std::basic_string_view<CharT> text, const std::vector<ItemLinkWire::WireLink>& links)
{
    std::vector<std::optional<Span>> spans(links.size());
    std::vector<std::size_t> order(links.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&links](std::size_t a, std::size_t b) { return links[a].offset < links[b].offset; });

    std::size_t searchFrom = 0;
    for (const std::size_t i : order)
    {
        const auto units = static_cast<std::size_t>(links[i].length);
        std::optional<Span> span;
        if (const auto start = IndexAfterUtf16(text, 0, static_cast<std::size_t>(links[i].offset));
            start && *start >= searchFrom)
        {
            span = BracketsAt(text, *start, units);
        }
        if (!span)
            span = SearchBrackets(text, searchFrom, units);
        if (!span)
            continue;
        spans[i] = span;
        searchFrom = span->start + span->length;
    }
    return spans;
}

// A shown text with each attached link's name replaced (the reader's own name of the item).
struct Renamed
{
    std::wstring text;
    std::vector<std::optional<Span>> spans;
};

// Puts names[i] where spans[i] stands (an empty name keeps the text there); spans must not overlap.
Renamed Rename(std::wstring_view text, const std::vector<std::optional<Span>>& spans,
               const std::vector<std::wstring>& names);

// Where a chat row that would break at `split` breaks instead so no link is cut: before the link it would cut, or
// after it when that link starts the text.
std::size_t KeepLinksWhole(std::size_t split, const std::vector<Span>& spans);

// A piece of a chat row: plain text, or the link of that index.
struct Segment
{
    std::wstring text;
    int link = -1;
};

// The row `lead` + `text` in pieces; spans (in order, relative to `text`) become links of their index in `spans`.
// A span out of order or past the end stays text.
std::vector<Segment> Segments(std::wstring_view lead, std::wstring_view text, const std::vector<Span>& spans);
} // namespace UI::Chat::ItemLinkAttach
