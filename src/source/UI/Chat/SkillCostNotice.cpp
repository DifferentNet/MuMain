// SkillCostNotice.cpp - "not enough mana / AG" when a skill cannot be paid.

#include "stdafx.h"

#include "UI/Chat/SkillCostNotice.h"

#include "GameLogic/Combat/SkillExecution.h"
#include "I18N/All.h"
#include "MUHelper/MuHelper.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/Core/WindowSystem.h"

namespace UI::Chat::SkillCost
{
namespace
{
SayGate g_gate;
} // namespace

void SayShort(bool manaShort)
{
    if (!g_gate.Allow(GetTickCount()))
    {
        return;
    }

    PostSystem(manaShort ? I18N::Game::TouchNoMana : I18N::Game::TouchNoAg, mu::ui::window::TYPE_ERROR_MESSAGE);
}

void NoteRefusedCast(int skill)
{
    if (MUHelper::g_MuHelper.IsActive())
    {
        return;
    }

    switch (GameLogic::Combat::SkillCostShortfall(skill))
    {
    case GameLogic::Combat::CostShortfall::Mana:
        // CheckMana has just asked for a potion if there is one: then the mana comes back by itself.
        if (g_pMyInventory->FindManaItemIndex() == -1)
        {
            SayShort(true);
        }
        break;
    case GameLogic::Combat::CostShortfall::Ag:
        SayShort(false);
        break;
    case GameLogic::Combat::CostShortfall::None:
        break;
    }
}
} // namespace UI::Chat::SkillCost
