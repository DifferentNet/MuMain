#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The lines typed into the chat bar (UI/HUD/ChatInputBox.cpp), for Up and Down while the field has focus. Up
// starts at the newest line and walks to older ones; Down walks back and, past the newest, returns the text that
// was in the field when browsing began. A line sent again moves to the newest place instead of repeating.
namespace UI::Chat
{
class ChatHistory
{
public:
    // Lines kept: the oldest goes when a new one comes past this.
    static constexpr std::size_t kCapacity = 30;

    // Stores a sent line as the newest and ends browsing. Empty lines are not kept.
    void Add(std::wstring_view line);
    // Removes the line at index (0 = oldest); ends browsing.
    void Remove(std::size_t index);
    void Clear();
    // Ends browsing: the next Up starts at the newest line again.
    void ResetBrowsing();

    // Up: the next older line to show, or nothing when there are no lines. The first Up keeps draft (the text in
    // the field) for Newer(); at the oldest line it stays there.
    std::optional<std::wstring> Older(std::wstring_view draft);
    // Down: the next newer line, the kept draft past the newest one (browsing ends), or nothing when not browsing.
    std::optional<std::wstring> Newer();

    const std::vector<std::wstring>& Lines() const { return m_lines; }

private:
    std::vector<std::wstring> m_lines; // oldest first
    std::size_t m_cursor = 0;          // == m_lines.size() while not browsing
    std::wstring m_draft;
    bool m_browsing = false;
};
} // namespace UI::Chat
