#include "stdafx.h"
#include "GameLogic/Character/HeroCombatPower.h"

#include "GameLogic/Character/AdvancedStats.h"
#include "GameLogic/Items/ItemCombatPower.h"
#include "Engine/Object/ZzzInfomation.h"
#include "UI/Inventory/InventoryCtrl.h"

#include <cmath>

namespace GameLogic::AdvancedStats
{
namespace
{
// How often an open window compares the equipment and the stats; a change asks the server again (at most every two
// seconds, Store::Request()).
constexpr auto InputCheckInterval = std::chrono::milliseconds(500);
} // namespace

std::optional<long long> KnownHeroCombatPower()
{
    const double* combatPower = Stats().Find(L"cp");
    if (combatPower == nullptr)
        return std::nullopt;
    return std::llround(*combatPower);
}

std::wstring FormatCombatPower(long long value)
{
    std::wstring digits = std::to_wstring(value < 0 ? -value : value);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
        digits.insert(static_cast<size_t>(i), L",");
    return value < 0 ? L"-" + digits : digits;
}

HeroCombatPowerWatch::Inputs HeroCombatPowerWatch::ReadInputs()
{
    Inputs inputs{};
    inputs[0] = GameLogic::Items::GetHeroEquipmentCombatPower().value_or(-1);
    if (CharacterAttribute != nullptr)
    {
        const CHARACTER_ATTRIBUTE& attribute = *CharacterAttribute;
        inputs[1] = attribute.Level;
        inputs[2] = attribute.Resets;
        inputs[3] = attribute.Strength;
        inputs[4] = attribute.Dexterity;
        inputs[5] = attribute.Vitality;
        inputs[6] = attribute.Energy;
        inputs[7] = attribute.Charisma;
        inputs[8] = attribute.AddStrength;
        inputs[9] = attribute.AddDexterity;
        inputs[10] = attribute.AddVitality;
        inputs[11] = attribute.AddEnergy;
        inputs[12] = attribute.AddCharisma;
    }
    inputs[13] = mu::ui::window::CInventoryCtrl::GetPickedItem() != nullptr ? 1 : 0;
    return inputs;
}

void HeroCombatPowerWatch::Update(bool shown)
{
    if (!shown)
    {
        m_shown = false;
        return;
    }

    auto& stats = Stats();
    const auto now = std::chrono::steady_clock::now();
    if (!m_shown)
    {
        m_shown = true;
        m_owedSince = now;
        m_nextInputCheck = {};
    }

    // An answer, or the store forgot the character (entering the world): a forgotten one is asked for again.
    if (stats.Version() != m_version)
    {
        m_version = stats.Version();
        if (!stats.HasInfo() && !m_owedSince)
            m_owedSince = now;
    }

    if (now >= m_nextInputCheck)
    {
        m_nextInputCheck = now + InputCheckInterval;
        const Inputs inputs = ReadInputs();
        if (m_inputs && *m_inputs != inputs && !m_owedSince)
            m_owedSince = now;
        m_inputs = inputs;
    }

    // Paid by any request sent since it was owed, this window's or another's (the advanced stats tab asks too).
    if (m_owedSince && (stats.LastRequest() >= *m_owedSince || stats.Request()))
        m_owedSince.reset();
}
} // namespace GameLogic::AdvancedStats
