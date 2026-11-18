#include "UI/Chat/PetLifeWarning.h"

namespace UI::Chat
{
PetLifeWarning::Say PetLifeWarning::Note(int before, int after)
{
    // The client keeps the pet's life only from these updates, so one that does not go on from the last
    // value belongs to another item (a pet swapped for one of the same kind): its warnings are new.
    if (m_lastAfter >= 0 && before != m_lastAfter)
    {
        Reset();
    }
    m_lastAfter = after;

    if (after > QuarterLife)
    {
        Reset();
        return Say::Nothing;
    }

    if (after > TenthLife)
    {
        m_saidTenth = false;
    }

    // At 0 the pet is gone and the "broken" line speaks; a rise (repair) or the first value says nothing.
    if (after <= 0 || before <= 0 || after >= before)
    {
        return Say::Nothing;
    }

    if (after <= TenthLife)
    {
        if (m_saidTenth)
        {
            return Say::Nothing;
        }

        m_saidTenth = true;
        m_saidQuarter = true;
        return Say::Tenth;
    }

    if (m_saidQuarter)
    {
        return Say::Nothing;
    }

    m_saidQuarter = true;
    return Say::Quarter;
}

void PetLifeWarning::Reset()
{
    m_saidQuarter = false;
    m_saidTenth = false;
}
} // namespace UI::Chat
