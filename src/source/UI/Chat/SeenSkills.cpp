#include "UI/Chat/SeenSkills.h"

namespace UI::Chat
{
void SeenSkills::BeginList(std::wstring_view name)
{
    if (name == m_name)
    {
        return;
    }

    m_name = name;
    m_seen.clear();
}

void SeenSkills::NoteKnown(int skillType)
{
    if (skillType != 0)
    {
        m_seen.insert(skillType);
    }
}

bool SeenSkills::NoteAdded(int skillType)
{
    return skillType != 0 && m_seen.insert(skillType).second;
}
} // namespace UI::Chat
