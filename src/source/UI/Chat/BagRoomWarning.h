#pragma once

// The bag fills up silently until pickups start failing, so a pickup that leaves AlmostFullCells free cells or fewer
// says so once, and one that fills the last cell says the bag is full. Both are said again only after the bag had
// more than AlmostFullCells free cells at a later pickup (a sale, a drop or the vault made room).
namespace UI::Chat
{
class BagRoomWarning
{
public:
    static constexpr int AlmostFullCells = 8;

    enum class Say
    {
        Nothing,
        AlmostFull,
        Full,
    };

    // The free cells after a pickup; what to say about them.
    [[nodiscard]] Say Note(int freeCells);

private:
    bool m_saidAlmostFull = false;
    bool m_saidFull = false;
};
} // namespace UI::Chat
