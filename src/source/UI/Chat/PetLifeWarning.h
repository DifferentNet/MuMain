#pragma once

// A pet or mount in the helper slot is destroyed at 0 life (all but the Dark Horse), and the "broken" line comes
// too late for it. So a hit that leaves QuarterLife or less says so once, and one that leaves TenthLife or less says
// it once more. Both are said again only after the life rose above the threshold (repair, another pet), or when
// an update does not start from the last life seen: another pet of the same kind went into the slot.
namespace UI::Chat
{
class PetLifeWarning
{
public:
    static constexpr int MaxLife = 255;
    static constexpr int QuarterLife = MaxLife / 4;  // 63
    static constexpr int TenthLife = MaxLife / 10;   // 25

    enum class Say
    {
        Nothing,
        Quarter,
        Tenth,
    };

    // The pet's life before and after a durability update; what to say about it.
    [[nodiscard]] Say Note(int before, int after);

    // Another pet (or none) went into the slot.
    void Reset();

private:
    bool m_saidQuarter = false;
    bool m_saidTenth = false;
    int m_lastAfter = -1;  // the life after the last update, -1 before the first
};
} // namespace UI::Chat
