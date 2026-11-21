#include "doctest.h"

#include "GameLogic/Items/ItemRarity.h"

using namespace GameLogic::Items::Rarity;

namespace
{
Facts Plain(int level = 0)
{
    Facts facts;
    facts.level = level;
    return facts;
}

Facts Excellent(int level = 0)
{
    Facts facts = Plain(level);
    facts.hasOptions = true;
    facts.excellentOptions = true;
    return facts;
}
} // namespace

TEST_CASE("each row of the name colour chain gives its tier [item_rarity]")
{
    Facts named = Plain();
    named.namedSpecial = true;
    CHECK(Classify(named) == Tier::Special);

    Facts divine = Plain();
    divine.divine = true;
    CHECK(Classify(divine) == Tier::Divine);

    Facts eventPart = Plain();
    eventPart.eventPart = true;
    CHECK(Classify(eventPart) == Tier::Special);

    Facts ancient = Plain();
    ancient.ancient = true;
    CHECK(Classify(ancient) == Tier::Ancient);

    Facts socket = Plain();
    socket.socket = true;
    CHECK(Classify(socket) == Tier::Socket);

    CHECK(Classify(Excellent()) == Tier::Excellent);
    CHECK(Classify(Plain(7)) == Tier::Fine);

    Facts magic = Plain();
    magic.hasOptions = true;
    CHECK(Classify(magic) == Tier::Magic);

    CHECK(Classify(Plain()) == Tier::Normal);
}

TEST_CASE("the first match of the chain wins [item_rarity]")
{
    Facts everything = Excellent(15);
    everything.namedSpecial = true;
    everything.divine = true;
    everything.eventPart = true;
    everything.ancient = true;
    everything.socket = true;
    CHECK(Classify(everything) == Tier::Special);

    everything.namedSpecial = false;
    CHECK(Classify(everything) == Tier::Divine);

    everything.divine = false;
    CHECK(Classify(everything) == Tier::Special);

    everything.eventPart = false;
    CHECK(Classify(everything) == Tier::Ancient);

    everything.ancient = false;
    CHECK(Classify(everything) == Tier::Socket);

    everything.socket = false;
    CHECK(Classify(everything) == Tier::Excellent);

    // Excellent flags without options are not excellent: +7 first, then plain.
    Facts flagsOnly = Plain(9);
    flagsOnly.excellentOptions = true;
    CHECK(Classify(flagsOnly) == Tier::Fine);
    flagsOnly.level = 3;
    CHECK(Classify(flagsOnly) == Tier::Normal);

    // +7 beats options.
    Facts magicFine = Plain(7);
    magicFine.hasOptions = true;
    CHECK(Classify(magicFine) == Tier::Fine);
}

TEST_CASE("wings and capes follow only level and options [item_rarity]")
{
    Facts wings = Excellent(3);
    wings.wingLike = true;
    wings.ancient = true;
    CHECK(Classify(wings) == Tier::Magic);

    wings.level = 7;
    CHECK(Classify(wings) == Tier::Fine);

    Facts plainWings = Plain(6);
    plainWings.wingLike = true;
    CHECK(Classify(plainWings) == Tier::Normal);
}

TEST_CASE("a jewel gem is always special [item_rarity]")
{
    Facts gem = Excellent(15);
    gem.ancient = true;
    gem.jewelGem = true;
    CHECK(Classify(gem) == Tier::Special);

    gem.wingLike = true;
    CHECK(Classify(gem) == Tier::Special);
}

TEST_CASE("upgrade bands split at +7 and +13 [item_rarity]")
{
    CHECK(BandOf(0) == UpgradeBand::None);
    CHECK(BandOf(6) == UpgradeBand::None);
    CHECK(BandOf(7) == UpgradeBand::Plus7);
    CHECK(BandOf(12) == UpgradeBand::Plus7);
    CHECK(BandOf(13) == UpgradeBand::Gold);
    CHECK(BandOf(15) == UpgradeBand::Gold);
}

TEST_CASE("each tier keeps its old name colour [item_rarity]")
{
    CHECK(ColorOf(Tier::Normal) == NameColor::White);
    CHECK(ColorOf(Tier::Magic) == NameColor::Blue);
    CHECK(ColorOf(Tier::Fine) == NameColor::Yellow);
    CHECK(ColorOf(Tier::Excellent) == NameColor::Green);
    CHECK(ColorOf(Tier::Ancient) == NameColor::GreenBlue);
    CHECK(ColorOf(Tier::Socket) == NameColor::Violet);
    CHECK(ColorOf(Tier::Special) == NameColor::Yellow);
    CHECK(ColorOf(Tier::Divine) == NameColor::Purple);
}

TEST_CASE("an unchanged name colour keeps the tier, a new one names its tier [item_rarity]")
{
    const Tier tiers[] = {Tier::Normal,  Tier::Magic,  Tier::Fine,    Tier::Excellent,
                          Tier::Ancient, Tier::Socket, Tier::Special, Tier::Divine};
    for (const Tier tier : tiers)
    {
        CAPTURE(static_cast<int>(tier));
        CHECK(FromFinalNameColor(tier, ColorOf(tier)) == tier);
    }

    CHECK(FromFinalNameColor(Tier::Excellent, NameColor::Blue) == Tier::Magic);
    CHECK(FromFinalNameColor(Tier::Normal, NameColor::Yellow) == Tier::Special);
    CHECK(FromFinalNameColor(Tier::Normal, NameColor::Violet) == Tier::Socket);
    CHECK(FromFinalNameColor(Tier::Normal, NameColor::Purple) == Tier::Divine);
    CHECK(FromFinalNameColor(Tier::Normal, NameColor::Green) == Tier::Excellent);
    CHECK(FromFinalNameColor(Tier::Normal, NameColor::GreenBlue) == Tier::Ancient);
    CHECK(FromFinalNameColor(Tier::Special, NameColor::White) == Tier::Normal);
    // Fine and Special share yellow: a yellow name keeps whichever the chain gave.
    CHECK(FromFinalNameColor(Tier::Fine, NameColor::Yellow) == Tier::Fine);
}
