#pragma once

// The skills a character had in this session, so "New skill: X" is said only for a skill he really did not have:
// taking a weapon with a skill off and on again, or the full list the server sends again, says nothing.
#include <string>
#include <string_view>
#include <unordered_set>

namespace UI::Chat
{
class SeenSkills
{
public:
    // A full skill list for the character `name` begins: another character forgets what the last one had.
    void BeginList(std::wstring_view name);

    // A skill of the full list: known, nothing to say.
    void NoteKnown(int skillType);

    // One skill was added. True when the character never had it in this session (it is known from now on).
    [[nodiscard]] bool NoteAdded(int skillType);

private:
    std::wstring m_name;
    std::unordered_set<int> m_seen;
};
} // namespace UI::Chat
