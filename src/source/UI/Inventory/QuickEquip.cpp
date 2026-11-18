#include "stdafx.h"

#include "UI/Inventory/QuickEquip.h"

#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Items/EquipOfferTracker.h"
#include "GameLogic/Items/ItemCombatPower.h"
#include "I18N/All.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/EquipSwap.h"
#include "UI/Inventory/InventoryContents.h"
#include "UI/Mobile/InterfaceMode.h"
#include "World/MapInfra/MapManager.h"

#include <string>

namespace
{
namespace EquipOffer = GameLogic::Items::EquipOffer;

constexpr std::size_t ItemNameSize = 128;
constexpr std::size_t OfferTextSize = 256;
constexpr int NoWorld = -1;

EquipOffer::EquipOfferTracker g_tracker;
std::wstring g_text;
// The map the offer came on: another map takes it away.
int g_world = NoWorld;

void SetText(const ITEM& item, int combatPowerChange)
{
    wchar_t name[ItemNameSize]{};
    GetItemName(item.Type, item.Level, name);
    wchar_t text[OfferTextSize]{};
    mu_swprintf(text, I18N::Game::PadOfferEquip, name, combatPowerChange);
    g_text = text;
}

// The offered item still in its cell, or nullptr.
const ITEM* OfferedItem(const EquipOffer::Offer& offer)
{
    const ITEM* item = UI::Inventory::FindPlayerItem(offer.bagIndex);
    return item != nullptr && item->Key == offer.itemKey ? item : nullptr;
}
} // namespace

namespace UI::Inventory::QuickEquip
{
void NoteItemPicked(int bagIndex)
{
    if (!UI::Mobile::ShowsControls() || Hero == nullptr || Hero->Dead != 0)
        return;
    const ITEM* item = FindPlayerItem(bagIndex);
    if (item == nullptr)
        return;
    const std::optional<int> change = GameLogic::Items::GetHeroCombatPowerChange(*item);
    if (!change)
        return;
    using EquipSwap::Refusal;
    const EquipSwap::Plan plan = EquipSwap::PlanFor(*item);
    EquipOffer::Candidate candidate;
    candidate.bagIndex = bagIndex;
    candidate.itemKey = item->Key;
    candidate.combatPowerChange = *change;
    candidate.canWear = plan.why != Refusal::CannotWear;
    candidate.canPutOnNow = plan.why == Refusal::None;
    if (!g_tracker.Consider(candidate, GetTickCount()))
        return;
    g_world = gMapManager.WorldActive;
    SetText(*item, *change);
}

const wchar_t* OfferText()
{
    return HasOffer() ? g_text.c_str() : nullptr;
}

bool HasOffer()
{
    return g_tracker.Current(GetTickCount()) != nullptr;
}

void AcceptTap()
{
    const EquipOffer::Offer* offer = g_tracker.Current(GetTickCount());
    if (offer != nullptr && EquipOffer::TakesTap(*offer, GetTickCount()))
        Accept();
}

bool Accept()
{
    const EquipOffer::Offer* offer = g_tracker.Current(GetTickCount());
    if (offer == nullptr)
        return false;
    const int bagIndex = offer->bagIndex;
    const bool stillThere = OfferedItem(*offer) != nullptr;
    g_tracker.Clear();
    // The player's own press; PutOn checks the item, the windows and the place again and says why it refuses. When
    // it can't even start (a window that takes bag items, a move or a sort waiting), the line says so.
    if (!stillThere || !EquipSwap::PutOn(bagIndex))
    {
        if (!g_pSystemLogBox->CheckChatRedundancy(I18N::Game::EquipNotNow))
            g_pSystemLogBox->AddText(I18N::Game::EquipNotNow, mu::ui::window::TYPE_ERROR_MESSAGE);
    }
    return true;
}

void Tick()
{
    const EquipOffer::Offer* offer = g_tracker.Current(GetTickCount());
    if (offer == nullptr)
        return;
    const bool heroGone = Hero == nullptr || Hero->Dead != 0;
    if (heroGone || gMapManager.WorldActive != g_world || !UI::Mobile::ShowsControls())
    {
        g_tracker.Clear();
        return;
    }
    if (OfferedItem(*offer) == nullptr)
        g_tracker.ItemLeft(offer->bagIndex);
}
} // namespace UI::Inventory::QuickEquip
