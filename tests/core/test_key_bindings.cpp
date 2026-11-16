#include <doctest.h>

#include "Core/Input/KeyBindings.h"

#include <set>

namespace KB = Core::Input::KeyBindings;
using KB::Action;

namespace
{
std::set<int> KeysOf(const KB::Table& table)
{
    std::set<int> keys;
    for (const KB::ActionInfo& info : KB::Actions())
        keys.insert(table.KeyOf(info.action));
    return keys;
}

bool IsCollisionFree(const KB::Table& table)
{
    const std::set<int> keys = KeysOf(table);
    if (static_cast<int>(keys.size()) != KB::ActionCount)
        return false;
    for (int key : keys)
    {
        if (!KB::IsAssignable(key))
            return false;
    }
    return true;
}

constexpr int KeyEscape = 0x1B;
constexpr int KeyF1 = 0x70;
} // namespace

TEST_CASE("the defaults are the game's old keys, all bindable and all different [core][keybindings]")
{
    const KB::Table table;
    CHECK(table.IsDefault());
    CHECK(IsCollisionFree(table));
    CHECK(table.KeyOf(Action::Inventory) == 'I');
    CHECK(table.KeyOf(Action::InventoryAlt) == 'V');
    CHECK(table.KeyOf(Action::MuHelperToggle) == KB::KeyHome);
    CHECK(table.KeyOf(Action::Potion1) == 'Q');
    CHECK(table.KeyOf(Action::Potion4) == 'R');
    CHECK(table.KeyOf(Action::SkillSlot1) == '1');
    CHECK(table.KeyOf(Action::SkillSlot0) == '0');
    for (const KB::ActionInfo& info : KB::Actions())
    {
        CHECK(table.KeyOf(info.action) == info.defaultKey);
        CHECK(table.ActionOf(info.defaultKey) == info.action);
    }
}

TEST_CASE("there are exactly as many bindable keys as actions [core][keybindings]")
{
    int assignable = 0;
    for (int key = 0; key < 256; ++key)
        assignable += KB::IsAssignable(key) ? 1 : 0;
    CHECK(assignable == KB::ActionCount);
    for (char reserved : std::string("AHKLSY"))
        CHECK_FALSE(KB::IsAssignable(reserved));
    CHECK_FALSE(KB::IsAssignable(KeyEscape));
    CHECK_FALSE(KB::IsAssignable(KeyF1));
    CHECK_FALSE(KB::IsAssignable('a'));
}

TEST_CASE("a taken key swaps the two actions [core][keybindings]")
{
    KB::Table table;
    const KB::Table::Change change = table.Assign(Action::Inventory, 'C');
    CHECK(change.first == Action::Inventory);
    CHECK(change.swapped == Action::Character);
    CHECK(table.KeyOf(Action::Inventory) == 'C');
    CHECK(table.KeyOf(Action::Character) == 'I');
    CHECK(table.ActionOf('C') == Action::Inventory);
    CHECK(table.ActionOf('I') == Action::Character);
    CHECK_FALSE(table.IsDefault());
    CHECK(IsCollisionFree(table));

    const KB::Table::Change cross = table.Assign(Action::SkillSlot3, KB::KeyHome);
    CHECK(cross.swapped == Action::MuHelperToggle);
    CHECK(table.KeyOf(Action::MuHelperToggle) == '3');
    CHECK(IsCollisionFree(table));
}

TEST_CASE("the same key, a fixed key or a reserved letter changes nothing [core][keybindings]")
{
    KB::Table table;
    for (int key : {static_cast<int>('I'), KeyEscape, KeyF1, static_cast<int>('K'), static_cast<int>('Y'), 0})
    {
        const KB::Table::Change change = table.Assign(Action::Inventory, key);
        CHECK(change.first == Action::None);
        CHECK(change.swapped == Action::None);
    }
    CHECK(table.IsDefault());
    CHECK(table.Assign(Action::None, 'C').first == Action::None);
}

TEST_CASE("reset brings the defaults back [core][keybindings]")
{
    KB::Table table;
    table.Assign(Action::Potion1, '5');
    table.Assign(Action::Move, 'Z');
    CHECK_FALSE(table.IsDefault());
    table.Reset();
    CHECK(table.IsDefault());
    CHECK(table == KB::Table());
}

TEST_CASE("only changed actions are saved and they read back the same [core][keybindings]")
{
    CHECK(KB::Table().ToEntries().empty());

    KB::Table table;
    table.Assign(Action::Inventory, 'B');
    table.Assign(Action::Potion2, '9');
    table.Assign(Action::MuHelperToggle, 'X');
    const KB::Table::Entries entries = table.ToEntries();
    CHECK(entries.size() == 6);
    CHECK(KB::Table::FromEntries(entries) == table);
}

TEST_CASE("broken saved entries are repaired without binding a key twice [core][keybindings]")
{
    SUBCASE("unknown names, unknown keys and fixed keys are skipped")
    {
        const KB::Table table = KB::Table::FromEntries(
            {{"Nonsense", "I"}, {"Inventory", "F1"}, {"Character", "K"}, {"Party", ""}, {"Guild", "Esc"}});
        CHECK(table.IsDefault());
    }
    SUBCASE("a key that an earlier action took goes to the earlier one")
    {
        const KB::Table table = KB::Table::FromEntries({{"Inventory", "C"}, {"Party", "C"}});
        CHECK(table.KeyOf(Action::Inventory) == 'C');
        CHECK(table.KeyOf(Action::Party) == 'P');
        // Character lost C; its default is taken, so it gets the lowest free key, I (the one Inventory left).
        CHECK(table.KeyOf(Action::Character) == 'I');
        CHECK(IsCollisionFree(table));
    }
    SUBCASE("a lost half of a swap is filled in")
    {
        const KB::Table table = KB::Table::FromEntries({{"SkillSlot1", "Home"}});
        CHECK(table.KeyOf(Action::SkillSlot1) == KB::KeyHome);
        CHECK(table.KeyOf(Action::MuHelperToggle) == '1');
        CHECK(IsCollisionFree(table));
    }
    SUBCASE("names and keys are read in any letter case")
    {
        const KB::Table table = KB::Table::FromEntries({{"inventory", "b"}, {"gens", "i"}});
        CHECK(table.KeyOf(Action::Inventory) == 'B');
        CHECK(table.KeyOf(Action::Gens) == 'I');
        CHECK(IsCollisionFree(table));
    }
}

TEST_CASE("key names read back to their keys [core][keybindings]")
{
    CHECK(KB::KeyName('I') == "I");
    CHECK(KB::KeyName('7') == "7");
    CHECK(KB::KeyName(KB::KeyHome) == "Home");
    CHECK(KB::KeyName(KeyF1).empty());
    for (int key = 0; key < 256; ++key)
    {
        if (!KB::IsAssignable(key))
            continue;
        CHECK(KB::KeyFromName(KB::KeyName(key)) == key);
    }
    CHECK(KB::KeyFromName("home") == KB::KeyHome);
    CHECK(KB::KeyFromName("HOME") == KB::KeyHome);
    CHECK(KB::KeyFromName("q") == 'Q');
    CHECK_FALSE(KB::KeyFromName("").has_value());
    CHECK_FALSE(KB::KeyFromName("Escape").has_value());
    CHECK_FALSE(KB::KeyFromName("!").has_value());
}

TEST_CASE("potion and skill slots map to their actions [core][keybindings]")
{
    CHECK(KB::PotionAction(0) == Action::Potion1);
    CHECK(KB::PotionAction(3) == Action::Potion4);
    CHECK(KB::PotionAction(-1) == Action::None);
    CHECK(KB::PotionAction(4) == Action::None);
    CHECK(KB::SkillSlotAction(0) == Action::SkillSlot0);
    CHECK(KB::SkillSlotAction(1) == Action::SkillSlot1);
    CHECK(KB::SkillSlotAction(9) == Action::SkillSlot9);
    CHECK(KB::SkillSlotAction(10) == Action::None);
    CHECK(KB::SkillSlotAction(-1) == Action::None);
    const KB::Table table;
    for (int hotKey = 0; hotKey < KB::SkillSlotCount; ++hotKey)
        CHECK(table.KeyOf(KB::SkillSlotAction(hotKey)) == '0' + hotKey);
}

TEST_CASE("a default key names its action [core][keybindings]")
{
    CHECK(KB::DefaultActionOf('I') == Action::Inventory);
    CHECK(KB::DefaultActionOf(KB::KeyHome) == Action::MuHelperToggle);
    CHECK(KB::DefaultActionOf('0') == Action::SkillSlot0);
    CHECK(KB::DefaultActionOf('Y') == Action::None);
    CHECK(KB::DefaultActionOf(KeyEscape) == Action::None);
}
