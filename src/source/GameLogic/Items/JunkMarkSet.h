#pragma once

#include "GameLogic/Items/JunkMarks.h"

#include <string>
#include <string_view>
#include <vector>

namespace GameLogic::Items::JunkMarks
{
// One junk mark: the bag cell (the protocol's slot index) and the fingerprint of the item marked there.
struct Mark
{
    int slot = -1;
    Fingerprint print = 0;
};

// One bag item: the slot it starts in and its fingerprint.
struct BagItem
{
    int slot = -1;
    Fingerprint print = 0;
};

// The junk marks of one character. A mark only counts while the item in its cell still has the marked fingerprint
// (IsMarked), so an item that took a freed cell is never sold by an old mark.
class JunkMarkSet
{
public:
    // The most marks kept: two bag pages of 64 cells.
    static constexpr std::size_t MaxMarks = 128;

    bool IsMarked(int slot, Fingerprint print) const;
    // Marks the item in slot (replaces an older mark there); false when the set is full.
    bool Mark(int slot, Fingerprint print);
    void Unmark(int slot);
    // The item in from moved to to: its mark goes with it, an old mark at to goes.
    void Move(int from, int to);
    // The bag as it stands now: a mark whose cell holds another item (or none) goes. With followSort (the list
    // answers a sort this client asked for) such a mark first follows an item with its fingerprint that no other
    // mark holds; items alike in everything the fingerprint sees are alike for the sale too.
    void Settle(const std::vector<BagItem>& bag, bool followSort);
    void Clear();

    std::size_t Count() const { return m_marks.size(); }
    const std::vector<JunkMarks::Mark>& All() const { return m_marks; }

    // "slot:fingerprint;..." with the fingerprint in hexadecimal; Parse skips what it can't read.
    std::string Serialize() const;
    static JunkMarkSet Parse(std::string_view text);

private:
    std::vector<JunkMarks::Mark> m_marks;
};
} // namespace GameLogic::Items::JunkMarks
