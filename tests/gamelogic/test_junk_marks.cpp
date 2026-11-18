#include <doctest.h>

#include "GameLogic/Items/JunkMarkSet.h"
#include "GameLogic/Items/JunkMarks.h"

#include <vector>

namespace JunkMarks = GameLogic::Items::JunkMarks;
using JunkMarks::Kind;
using JunkMarks::Traits;

namespace
{
Traits Plain(Kind kind, int level = 0)
{
    Traits traits;
    traits.kind = kind;
    traits.level = level;
    return traits;
}

JunkMarks::Identity Sword(int level = 0)
{
    JunkMarks::Identity identity;
    identity.type = 3;
    identity.level = level;
    return identity;
}
} // namespace

TEST_CASE("plain weapons, shields and armour below +7 may be junk [gamelogic][junk]")
{
    CHECK(JunkMarks::CanBeJunk(Plain(Kind::Weapon)));
    CHECK(JunkMarks::CanBeJunk(Plain(Kind::Shield, 3)));
    CHECK(JunkMarks::CanBeJunk(Plain(Kind::Armour, 6)));
    CHECK_FALSE(JunkMarks::CanBeJunk(Plain(Kind::Armour, 7)));
    CHECK_FALSE(JunkMarks::CanBeJunk(Plain(Kind::Weapon, 15)));
}

TEST_CASE("rings, pendants, wings, jewels and other items are never junk [gamelogic][junk]")
{
    CHECK_FALSE(JunkMarks::CanBeJunk(Plain(Kind::Other)));
}

TEST_CASE("excellent, ancient, lucky, socketed, reinforced, valuable, unsellable or mix gear is never junk [gamelogic][junk]")
{
    auto check = [](auto&& change)
    {
        Traits traits = Plain(Kind::Weapon);
        change(traits);
        return JunkMarks::CanBeJunk(traits);
    };
    CHECK_FALSE(check([](Traits& t) { t.excellent = true; }));
    CHECK_FALSE(check([](Traits& t) { t.ancient = true; }));
    CHECK_FALSE(check([](Traits& t) { t.luck = true; }));
    CHECK_FALSE(check([](Traits& t) { t.sockets = true; }));
    CHECK_FALSE(check([](Traits& t) { t.harmony = true; }));
    CHECK_FALSE(check([](Traits& t) { t.sellable = false; }));
    CHECK_FALSE(check([](Traits& t) { t.highValue = true; }));
    CHECK_FALSE(check([](Traits& t) { t.questOrEvent = true; }));
    CHECK_FALSE(check([](Traits& t) { t.mixIngredient = true; }));
}

TEST_CASE("the fingerprint tells items apart but not their wear [gamelogic][junk]")
{
    CHECK(JunkMarks::MakeFingerprint(Sword()) == JunkMarks::MakeFingerprint(Sword()));
    CHECK(JunkMarks::MakeFingerprint(Sword()) != JunkMarks::MakeFingerprint(Sword(1)));
    JunkMarks::Identity skilled = Sword();
    skilled.skill = true;
    CHECK(JunkMarks::MakeFingerprint(Sword()) != JunkMarks::MakeFingerprint(skilled));
    JunkMarks::Identity option = Sword();
    option.optionLevel = 1;
    CHECK(JunkMarks::MakeFingerprint(Sword()) != JunkMarks::MakeFingerprint(option));
    CHECK(JunkMarks::MakeFingerprint(Sword()) != 0);
}

TEST_CASE("a mark counts only for the item it was set on [gamelogic][junk]")
{
    JunkMarks::JunkMarkSet marks;
    const auto sword = JunkMarks::MakeFingerprint(Sword());
    const auto other = JunkMarks::MakeFingerprint(Sword(2));
    CHECK(marks.Mark(20, sword));
    CHECK(marks.IsMarked(20, sword));
    // Another item in the same cell: not marked.
    CHECK_FALSE(marks.IsMarked(20, other));
    CHECK_FALSE(marks.IsMarked(21, sword));
    // A new mark in the cell replaces the old one.
    CHECK(marks.Mark(20, other));
    CHECK(marks.Count() == 1);
    marks.Unmark(20);
    CHECK(marks.Count() == 0);
    CHECK_FALSE(marks.Mark(-1, sword));
    CHECK_FALSE(marks.Mark(20, 0));
}

TEST_CASE("a mark follows its item to another cell [gamelogic][junk]")
{
    JunkMarks::JunkMarkSet marks;
    const auto sword = JunkMarks::MakeFingerprint(Sword());
    const auto other = JunkMarks::MakeFingerprint(Sword(3));
    marks.Mark(20, sword);
    marks.Mark(30, other);
    marks.Move(20, 30);
    CHECK(marks.IsMarked(30, sword));
    CHECK_FALSE(marks.IsMarked(30, other));
    CHECK_FALSE(marks.IsMarked(20, sword));
    CHECK(marks.Count() == 1);
}

TEST_CASE("a list the client didn't sort only drops marks whose cell holds another item [gamelogic][junk]")
{
    JunkMarks::JunkMarkSet marks;
    const auto sword = JunkMarks::MakeFingerprint(Sword());
    const auto other = JunkMarks::MakeFingerprint(Sword(3));
    marks.Mark(20, sword);
    marks.Mark(30, sword);
    marks.Mark(40, other);
    // 20 keeps its sword; 30 is empty now; another sword sits at 50 and an unmarked item at 40.
    const std::vector<JunkMarks::BagItem> bag = {{20, sword}, {40, sword}, {50, sword}};
    marks.Settle(bag, false);
    CHECK(marks.IsMarked(20, sword));
    CHECK_FALSE(marks.IsMarked(40, sword));
    CHECK_FALSE(marks.IsMarked(50, sword));
    CHECK(marks.Count() == 1);
}

TEST_CASE("after the client's own sort marks follow items alike that no other mark holds [gamelogic][junk]")
{
    JunkMarks::JunkMarkSet marks;
    const auto sword = JunkMarks::MakeFingerprint(Sword());
    const auto other = JunkMarks::MakeFingerprint(Sword(3));
    marks.Mark(20, sword);
    marks.Mark(30, sword);
    marks.Mark(60, other);
    // The sort put the swords at 12 and 20, the other item nowhere.
    const std::vector<JunkMarks::BagItem> bag = {{12, sword}, {20, sword}, {13, JunkMarks::MakeFingerprint(Sword(5))}};
    marks.Settle(bag, true);
    CHECK(marks.IsMarked(20, sword));
    CHECK(marks.IsMarked(12, sword));
    CHECK_FALSE(marks.IsMarked(13, other));
    CHECK(marks.Count() == 2);
}

TEST_CASE("the set holds a bounded number of marks [gamelogic][junk]")
{
    JunkMarks::JunkMarkSet marks;
    for (int slot = 0; slot < static_cast<int>(JunkMarks::JunkMarkSet::MaxMarks); ++slot)
        CHECK(marks.Mark(slot, 1));
    CHECK_FALSE(marks.Mark(1000, 1));
    // Replacing a mark in a taken cell still works.
    CHECK(marks.Mark(5, 2));
}

TEST_CASE("marks survive saving and skip broken entries [gamelogic][junk]")
{
    JunkMarks::JunkMarkSet marks;
    marks.Mark(12, 0xABCDEF0123456789ull);
    marks.Mark(80, 7);
    const std::string text = marks.Serialize();
    const auto loaded = JunkMarks::JunkMarkSet::Parse(text);
    CHECK(loaded.Count() == 2);
    CHECK(loaded.IsMarked(12, 0xABCDEF0123456789ull));
    CHECK(loaded.IsMarked(80, 7));

    const auto broken = JunkMarks::JunkMarkSet::Parse("x:1;13:zz;14;-2:5;15:0;16:a;17:b");
    CHECK(broken.Count() == 2);
    CHECK(broken.IsMarked(16, 0xa));
    CHECK(broken.IsMarked(17, 0xb));
    CHECK(JunkMarks::JunkMarkSet::Parse("").Count() == 0);
}
