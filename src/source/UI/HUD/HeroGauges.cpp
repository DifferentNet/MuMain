#include "stdafx.h"
#include "UI/HUD/HeroGauges.h"

#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Network/Server/WSclient.h"

#include <algorithm>

namespace UI::HUD::HeroGauges
{
namespace
{
bool IsMaster()
{
    return gCharacterManager.IsMasterLevel(Hero->Class);
}

// Empty below 0, full above the maximum.
Gauge Clamped(int current, int max)
{
    const int top = std::max(0, max);
    return {static_cast<std::uint32_t>(std::clamp(current, 0, top)), static_cast<std::uint32_t>(top)};
}

// The experience a level starts at: the server's curve, (9 + L) * L^2 * 10 below level 256, plus
// (9 + O) * O^2 * 1000 for the levels O over 255. Master levels count on from level 400 the same
// way, halved and less the experience of level 400 itself.
std::int64_t LevelStart(std::int64_t level, bool master)
{
    if (master)
    {
        const std::int64_t totalLevel = level + 400;
        const std::int64_t overLevel = totalLevel - 255;
        const std::int64_t masterData = ((9 + totalLevel) * totalLevel * totalLevel * 10)
                                        + ((9 + overLevel) * overLevel * overLevel * 1000);
        return (masterData - 3892250000LL) / 2;
    }

    const std::int64_t priorLevel = level - 1;
    if (priorLevel <= 0)
        return 0;
    std::int64_t start = (9 + priorLevel) * priorLevel * priorLevel * 10;
    if (priorLevel > 255)
    {
        const std::int64_t overLevel = priorLevel - 255;
        start += (9 + overLevel) * overLevel * overLevel * 1000;
    }
    return start;
}
} // namespace

Gauge Life()
{
    const int max = IsMaster() ? Master_Level_Data.wMaxLife : CharacterAttribute->LifeMax;
    return Clamped(CharacterAttribute->Life, max);
}

Gauge Mana()
{
    const int max = IsMaster() ? Master_Level_Data.wMaxMana : CharacterAttribute->ManaMax;
    return Clamped(CharacterAttribute->Mana, max);
}

Gauge SkillMana()
{
    const int max = std::max<int>(1, IsMaster() ? Master_Level_Data.wMaxBP : CharacterAttribute->SkillManaMax);
    return Clamped(CharacterAttribute->SkillMana, max);
}

Gauge Shield()
{
    const int max = std::max<int>(1, IsMaster() ? Master_Level_Data.wMaxShield : CharacterAttribute->ShieldMax);
    return Clamped(CharacterAttribute->Shield, max);
}

Experience ReadExperience()
{
    const bool master = gCharacterManager.IsMasterExperienceActive(CharacterAttribute->Class, CharacterAttribute->Level);

    Experience experience;
    if (master)
    {
        experience.level = static_cast<std::int64_t>(Master_Level_Data.nMLevel);
        experience.next = static_cast<std::int64_t>(Master_Level_Data.lNext_MasterLevel_Experince);
        experience.current = static_cast<std::int64_t>(Master_Level_Data.lMasterLevel_Experince);
    }
    else
    {
        experience.level = CharacterAttribute->Level;
        experience.next = CharacterAttribute->NextExperience;
        experience.current = CharacterAttribute->Experience;
    }

    const std::int64_t start = LevelStart(experience.level, master);
    const std::int64_t end = std::max(experience.next, start);
    const double needed = static_cast<double>(end - start);
    const double reached = std::clamp(static_cast<double>(experience.current), static_cast<double>(start),
                                      static_cast<double>(end));
    experience.progress = needed > 0.0 ? std::clamp((reached - static_cast<double>(start)) / needed, 0.0, 1.0) : 0.0;
    return experience;
}
} // namespace UI::HUD::HeroGauges
