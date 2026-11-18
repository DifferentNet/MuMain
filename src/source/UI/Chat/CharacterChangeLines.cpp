#include "stdafx.h"
#include "UI/Chat/CharacterChangeLines.h"

#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Combat/DuelMgr.h"
#include "GameLogic/Items/JewelUpgradeResult.h"
#include "I18N/All.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/Chat/ZenChangeWindow.h"
#include "UI/HUD/KeyHint.h"
#include "UI/Mobile/InterfaceMode.h"
#include "UI/Text/GroupedNumberText.h"
#include "World/MapInfra/MapManager.h"

namespace UI::Chat::ChangeLines
{
namespace
{
using UI::Chat::ZenChangeWindow;

// A sale's money update and its answer come at once; several quick sales give one line.
ZenChangeWindow g_sale{ZenChangeWindow::Sign::Gain, {0, 3'000, 400}};
// "Repair all" answers with a money update per repaired item; the first comes within a second or not at all (a
// refused repair sends nothing).
ZenChangeWindow g_repair{ZenChangeWindow::Sign::Loss, {0, 3'000, 500, 1'000}};
// A death's loss comes just before or just after the death packet.
ZenChangeWindow g_death{ZenChangeWindow::Sign::Loss, {1'500, 2'500, 600}};

// The respawn packet carries the experience the death left; it comes a few seconds after the death.
constexpr std::uint32_t RevivalWithinMs = 60'000;
bool g_deathCounts = false;
std::uint32_t g_diedAt = 0;

constexpr std::uint32_t LevelUpLineEveryMs = 60'000;
bool g_levelLineSaid = false;
std::uint32_t g_levelLineAt = 0;

void PostZen(const wchar_t* format, std::int64_t amount, mu::ui::window::MESSAGE_TYPE kind)
{
    wchar_t text[128]{};
    mu_swprintf(text, format, UI::Text::GroupThousands(amount).c_str());
    PostSystem(text, kind);
}

// Where a death costs nothing: a duel and the event maps (the server takes nothing in mini-games).
bool DeathCostsNothingHere()
{
    return g_DuelMgr.IsDuelEnabled() || gMapManager.InChaosCastle() || gMapManager.InBloodCastle() ||
           gMapManager.InDevilSquare() || gMapManager.IsCursedTemple() || gMapManager.IsEmpireGuardian();
}

// The key of the character window, or nothing where there is no keyboard to press it on (touch, the phone view).
std::wstring CharacterKey()
{
    if (UI::Mobile::IsTouchControls() || UI::Mobile::IsPhoneView())
        return {};
    return UI::KeyHint::KeyLabel(Core::Input::KeyBindings::Action::Character);
}
} // namespace

void PostJewelUpgrade(const ITEM& before, const ITEM& after)
{
    using namespace GameLogic::Items::JewelUpgrade;
    const Outcome outcome = Compare({before.Level, before.OptionLevel}, {after.Level, after.OptionLevel});
    const wchar_t* name = ItemAttribute[after.Type].Name;

    wchar_t text[160]{};
    switch (outcome)
    {
    case Outcome::LevelUp:
        mu_swprintf(text, I18N::Game::JewelUpgradedTo, name, after.Level);
        PostSystem(text, mu::ui::window::TYPE_SYSTEM_MESSAGE);
        break;
    case Outcome::LevelDown:
        mu_swprintf(text, I18N::Game::JewelUpgradeFailed, name, after.Level);
        PostSystem(text, mu::ui::window::TYPE_ERROR_MESSAGE);
        break;
    case Outcome::OptionUp:
        mu_swprintf(text, I18N::Game::JewelOptionImproved, name);
        PostSystem(text, mu::ui::window::TYPE_SYSTEM_MESSAGE);
        break;
    case Outcome::OptionDown:
        mu_swprintf(text, I18N::Game::JewelOptionFailed, name);
        PostSystem(text, mu::ui::window::TYPE_ERROR_MESSAGE);
        break;
    case Outcome::None:
        break;
    }
}

bool NoteZenChange(std::int64_t delta)
{
    const std::uint32_t now = GetTickCount();
    // The first window that counts a change takes it, so a change is never said twice.
    return g_sale.Note(delta, now) || g_repair.Note(delta, now) || g_death.Note(delta, now);
}

void NoteSaleAsked()
{
    g_sale.Arm(GetTickCount());
}

void NoteSaleRefused()
{
    g_sale.Stop();
}

void NoteRepairAsked()
{
    g_repair.Arm(GetTickCount());
}

void NoteRepairAsked(const ITEM& item)
{
    if (item.Type < 0 || item.Durability >= CalcMaxDurability(&item, &ItemAttribute[item.Type], item.Level))
        return;
    NoteRepairAsked();
}

void NoteHeroDeath()
{
    g_deathCounts = !DeathCostsNothingHere();
    g_diedAt = GetTickCount();
    if (g_deathCounts)
        g_death.Arm(g_diedAt);
}

void NoteRevivalExperience(std::int64_t before, std::int64_t after)
{
    const bool afterDeath = g_deathCounts && GetTickCount() - g_diedAt <= RevivalWithinMs;
    g_deathCounts = false;
    if (!afterDeath || after >= before)
        return;

    wchar_t text[128]{};
    mu_swprintf(text, I18N::Game::DeathLostExperience, UI::Text::GroupThousands(before - after).c_str());
    PostSystem(text, mu::ui::window::TYPE_ERROR_MESSAGE);
}

void NoteLevelUp(int level, int freePoints, bool reset)
{
    if (freePoints <= 0)
        return;

    const std::uint32_t now = GetTickCount();
    if (!reset && g_levelLineSaid && now - g_levelLineAt < LevelUpLineEveryMs)
        return;

    const std::wstring key = CharacterKey();
    wchar_t text[160]{};
    if (reset && key.empty())
        mu_swprintf(text, I18N::Game::ResetFreePointsNoKey, freePoints);
    else if (reset)
        mu_swprintf(text, I18N::Game::ResetFreePoints, freePoints, key.c_str());
    else if (key.empty())
        mu_swprintf(text, I18N::Game::LevelUpFreePointsNoKey, level, freePoints);
    else
        mu_swprintf(text, I18N::Game::LevelUpFreePoints, level, freePoints, key.c_str());

    g_levelLineSaid = true;
    g_levelLineAt = now;
    PostSystem(text, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

void Tick()
{
    const std::uint32_t now = GetTickCount();
    if (const auto sold = g_sale.Poll(now))
        PostZen(I18N::Game::ZenSold, *sold, mu::ui::window::TYPE_SYSTEM_MESSAGE);
    if (const auto repaired = g_repair.Poll(now))
        PostZen(I18N::Game::ZenRepairCost, *repaired, mu::ui::window::TYPE_SYSTEM_MESSAGE);
    if (const auto lost = g_death.Poll(now))
        PostZen(I18N::Game::DeathLostZen, *lost, mu::ui::window::TYPE_ERROR_MESSAGE);
}
} // namespace UI::Chat::ChangeLines
