#pragma once

#include "UI/Chat/ItemLinkWire.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace UI::Chat
{
// The "#ILK" lines (UI/Chat/ItemLinkWire.h) waiting for their chat line: the server sends one right before the chat
// line of that sender and channel, which takes it. One that nothing took (a whisper the player blocks) goes after
// KeptMs. Times are milliseconds of one monotonic clock.
class ItemLinkPending
{
public:
    static constexpr std::uint64_t KeptMs = 3'000;
    // More waiting at once than any chat brings in KeptMs; the oldest goes first.
    static constexpr std::size_t MaxWaiting = 16;

    // Keeps the line; one of the same sender and channel still waiting is replaced.
    void Put(ItemLinkWire::LinksLine line, std::uint64_t nowMs);
    // The line waiting for this sender and channel, taken out; empty when none is.
    std::optional<ItemLinkWire::LinksLine> Take(std::wstring_view sender, wchar_t channel, std::uint64_t nowMs);
    void Clear();
    std::size_t Waiting() const { return m_entries.size(); }

private:
    struct Entry
    {
        ItemLinkWire::LinksLine line;
        std::uint64_t arrivedMs = 0;
    };

    void DropExpired(std::uint64_t nowMs);

    std::vector<Entry> m_entries;
};
} // namespace UI::Chat
