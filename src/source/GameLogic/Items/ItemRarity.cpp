#include "GameLogic/Items/ItemRarity.h"

namespace GameLogic::Items::Rarity
{
namespace
{
constexpr int FineLevel = 7;
constexpr int GoldLevel = 13;

// Wings and capes: +7 first, then options, then plain.
Tier ClassifyByLevelAndOptions(const Facts& facts)
{
    if (facts.level >= FineLevel)
        return Tier::Fine;
    return facts.hasOptions ? Tier::Magic : Tier::Normal;
}

Tier ClassifyChain(const Facts& facts)
{
    if (facts.namedSpecial)
        return Tier::Special;
    if (facts.divine)
        return Tier::Divine;
    if (facts.eventPart)
        return Tier::Special;
    if (facts.ancient)
        return Tier::Ancient;
    if (facts.socket)
        return Tier::Socket;
    if (facts.hasOptions && facts.excellentOptions)
        return Tier::Excellent;
    return ClassifyByLevelAndOptions(facts);
}
} // namespace

Tier Classify(const Facts& facts)
{
    Tier tier = facts.wingLike ? ClassifyByLevelAndOptions(facts) : ClassifyChain(facts);
    if (facts.jewelGem)
        tier = Tier::Special;
    return tier;
}

NameColor ColorOf(Tier tier)
{
    switch (tier)
    {
    case Tier::Magic:
        return NameColor::Blue;
    case Tier::Fine:
    case Tier::Special:
        return NameColor::Yellow;
    case Tier::Excellent:
        return NameColor::Green;
    case Tier::Ancient:
        return NameColor::GreenBlue;
    case Tier::Socket:
        return NameColor::Violet;
    case Tier::Divine:
        return NameColor::Purple;
    case Tier::Normal:
    default:
        return NameColor::White;
    }
}

Tier FromFinalNameColor(Tier initial, NameColor finalColor)
{
    if (finalColor == ColorOf(initial))
        return initial;

    switch (finalColor)
    {
    case NameColor::Yellow:
        return Tier::Special;
    case NameColor::Violet:
        return Tier::Socket;
    case NameColor::Purple:
        return Tier::Divine;
    case NameColor::Green:
        return Tier::Excellent;
    case NameColor::GreenBlue:
        return Tier::Ancient;
    case NameColor::Blue:
        return Tier::Magic;
    case NameColor::White:
    default:
        return Tier::Normal;
    }
}

UpgradeBand BandOf(int level)
{
    if (level >= GoldLevel)
        return UpgradeBand::Gold;
    if (level >= FineLevel)
        return UpgradeBand::Plus7;
    return UpgradeBand::None;
}
} // namespace GameLogic::Items::Rarity
