#include "UI/Chat/ChatRowWrap.h"

#include <algorithm>

namespace UI::Chat
{
namespace
{
// The first half of a UTF-16 pair (wchar_t is 16 bits on Windows): a row never ends between the two halves.
bool IsLeadSurrogate(wchar_t c)
{
    return c >= 0xD800 && c <= 0xDBFF;
}
} // namespace

float ChatRowWidth(float room)
{
    return room - std::max(MinRowMarginPx, room * RowMarginShare);
}

bool ChatRowTooNarrow(float rowWidth, const MeasureChatRow& measure)
{
    return rowWidth < measure(MinRowSample);
}

ChatRowSource ChooseChatRowSource(bool rmlUi, bool logUpdated, bool hasProbe, float room)
{
    if (!rmlUi)
        return ChatRowSource::NativeFont;
    if (!logUpdated)
        return ChatRowSource::Wait;
    if (!hasProbe)
        return ChatRowSource::NativeFont;
    return room > 0.f ? ChatRowSource::LogFont : ChatRowSource::Wait;
}

std::vector<std::wstring> WrapChatRows(const std::wstring& prefix, const std::wstring& text, float rowWidth,
                                       const MeasureChatRow& measure)
{
    std::vector<std::wstring> rows;
    for (const ChatTextSpan& row : WrapChatRowSpans(prefix, text, rowWidth, measure))
        rows.push_back(text.substr(row.start, row.length));
    return rows;
}

std::vector<ChatTextSpan> WrapChatRowSpans(const std::wstring& prefix, const std::wstring& text, float rowWidth,
                                           const MeasureChatRow& measure, const std::vector<ChatTextSpan>& keepWhole)
{
    std::vector<ChatTextSpan> rows;
    std::size_t start = 0;
    // Whether text[start, end) fits the row being filled; the first row is measured with the prefix.
    const auto fits = [&](std::size_t end)
    {
        const std::wstring row = text.substr(start, end - start);
        return measure(rows.empty() ? prefix + row : row) <= rowWidth;
    };
    // The piece kept whole that a row ending at `at` would cut, if any.
    const auto cutPiece = [&](std::size_t at) -> const ChatTextSpan*
    {
        for (const ChatTextSpan& piece : keepWhole)
        {
            if (piece.start < at && at < piece.start + piece.length)
                return &piece;
        }
        return nullptr;
    };

    while (start < text.size())
    {
        if (fits(text.size()))
        {
            rows.push_back({start, text.size() - start});
            return rows;
        }

        // The most whole words that fit: the row ends where a word ends, before a space.
        std::size_t end = start;
        for (std::size_t space = text.find(L' ', start); space != std::wstring::npos;
             space = text.find(L' ', space + 1))
        {
            if (space == start || text[space - 1] == L' ')
                continue; // spaces in a row: only the first one ends a word
            if (cutPiece(space) != nullptr)
                continue; // a space inside an item's name ends no word
            if (!fits(space))
                break;
            end = space;
        }

        // Not even the first word fits after the sender: the sender's row ends there with no text, and the word
        // starts the next row as it would after any other word.
        const bool senderAlone = end == start && rows.empty() && !prefix.empty() && measure(prefix) <= rowWidth;
        if (end == start && !senderAlone)
        {
            // Not even the first word fits a row of its own (or the sender alone is wider than a row): it
            // breaks after the most characters that do, at least one.
            end = start + 1;
            while (end < text.size() && fits(end + 1))
                ++end;
            if (end < text.size() && IsLeadSurrogate(text[end - 1]))
                end = end - 1 > start ? end - 1 : end + 1;
            // Before an item's name it would cut, unless the row starts with that name (wider than a row).
            if (const ChatTextSpan* piece = cutPiece(end); piece != nullptr && piece->start > start)
                end = piece->start;
        }

        rows.push_back({start, end - start});
        start = end;
        while (start < text.size() && text[start] == L' ')
            ++start;
    }

    if (rows.empty())
        rows.push_back({0, 0});
    return rows;
}
} // namespace UI::Chat
