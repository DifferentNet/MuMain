#include "GameLogic/Items/JunkMarks.h"

namespace GameLogic::Items::JunkMarks
{
namespace
{
// FNV-1a, 64 bits.
constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

void Mix(std::uint64_t& hash, int value)
{
    auto bits = static_cast<std::uint32_t>(value);
    for (int byte = 0; byte < 4; ++byte)
    {
        hash ^= bits & 0xFFu;
        hash *= FnvPrime;
        bits >>= 8;
    }
}

bool IsGear(Kind kind)
{
    return kind == Kind::Weapon || kind == Kind::Shield || kind == Kind::Armour;
}
} // namespace

bool CanBeJunk(const Traits& traits)
{
    if (!IsGear(traits.kind) || !traits.sellable || traits.questOrEvent || traits.mixIngredient)
        return false;
    if (traits.excellent || traits.ancient || traits.luck || traits.sockets || traits.harmony)
        return false;
    return !traits.highValue && traits.level < FirstValuableLevel;
}

Fingerprint MakeFingerprint(const Identity& identity)
{
    std::uint64_t hash = FnvOffset;
    Mix(hash, identity.type);
    Mix(hash, identity.level);
    Mix(hash, identity.optionLevel);
    Mix(hash, identity.excellentFlags);
    Mix(hash, identity.ancient);
    Mix(hash, identity.luck ? 1 : 0);
    Mix(hash, identity.skill ? 1 : 0);
    Mix(hash, identity.harmonyOption);
    Mix(hash, identity.socketCount);
    return hash == 0 ? 1 : hash;
}
} // namespace GameLogic::Items::JunkMarks
