#include "stdafx.h"
#include "UI/Chat/HeroStatusLines.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"
#include "UI/Chat/BagRoomWarning.h"
#include "UI/Chat/BuffEndNotices.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/Chat/PetLifeWarning.h"
#include "UI/Inventory/InventoryContents.h"
#include "World/MapInfra/MapManager.h"

#include <list>
#include <string>

namespace UI::Chat::StatusLines
{
namespace
{
BuffEndNotices g_buffEnds;
BagRoomWarning g_bagRoom;
PetLifeWarning g_petLife;
int g_petLifeType = -1;

// The map the last Tick() saw, so entering another one silences the buff ends it brings.
bool g_worldSeen = false;
int g_world = 0;

// Buffs of the hero that are no player's skill or item effect: a siege formation, the GM mark.
bool IsSilentBuff(eBuffState buff)
{
    return buff == eBuff_CastleRegimentDefense || buff == eBuff_CastleRegimentAttack1 ||
           buff == eBuff_CastleRegimentAttack2 || buff == eBuff_CastleRegimentAttack3 || buff == eBuff_GMEffect;
}

// The buff's name: the first row of its tooltip (UI/HUD/BuffStrip.cpp); empty when it has none.
std::wstring BuffName(eBuffState buff)
{
    std::list<std::wstring> rows;
    g_BuffToolTipString(rows, buff);
    return rows.empty() ? std::wstring() : rows.front();
}

void PostBuffsEnded(const std::vector<int>& buffs)
{
    std::wstring names;
    for (const int buff : buffs)
    {
        const std::wstring name = BuffName(static_cast<eBuffState>(buff));
        if (name.empty())
            continue;
        if (!names.empty())
            names += L", ";
        names += name;
    }
    if (names.empty())
        return;

    wchar_t line[256]{};
    mu_swprintf(line, I18N::Game::BuffEnded, names.c_str());
    PostSystem(line, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

// Arrows and bolts in the hands count their shots as durability: their own lines cover them.
bool IsAmmunition(int itemType)
{
    return itemType == ITEM_ARROWS || itemType == ITEM_BOLT;
}

void TrackWorld(std::uint32_t now)
{
    const int world = gMapManager.WorldActive;
    if (g_worldSeen && world == g_world)
        return;
    if (g_worldSeen)
        g_buffEnds.Silence(now);
    g_worldSeen = true;
    g_world = world;
}
} // namespace

void NoteHeroBuffEnded(int buff)
{
    const auto state = static_cast<eBuffState>(buff);
    if (Hero == nullptr || Hero->Dead != 0 || IsSilentBuff(state))
        return;
    // Debuffs (poison, freeze...) come and go all the time in a fight.
    if (g_IsBuffClass(state) != eBuffClass_Buff)
        return;
    g_buffEnds.Ended(buff, GetTickCount());
}

void NoteHeroBuffStarted(int buff)
{
    g_buffEnds.Started(buff);
}

void NoteItemPickedUp()
{
    const int freeCells = UI::Inventory::FreeBagCells();
    switch (g_bagRoom.Note(freeCells))
    {
    case BagRoomWarning::Say::AlmostFull:
    {
        wchar_t line[128]{};
        mu_swprintf(line, I18N::Game::BagAlmostFull, freeCells);
        PostSystem(line, mu::ui::window::TYPE_ERROR_MESSAGE);
        break;
    }
    case BagRoomWarning::Say::Full:
        PostSystem(I18N::Game::InventoryIsFull, mu::ui::window::TYPE_ERROR_MESSAGE);
        break;
    default:
        break;
    }
}

// The pet or mount in the helper slot: warn at a quarter and a tenth of its life (the Dark Horse is never lost).
void NotePetLife(int before, int after)
{
    const int itemType = CharacterMachine->Equipment[EQUIPMENT_HELPER].Type;
    if (itemType != g_petLifeType)
    {
        g_petLifeType = itemType;
        g_petLife.Reset();
    }
    if (itemType < 0 || itemType == ITEM_DARK_HORSE_ITEM)
        return;

    if (g_petLife.Note(before, after) == PetLifeWarning::Say::Nothing)
        return;

    wchar_t line[192]{};
    mu_swprintf(line, I18N::Game::PetLifeLow, ItemAttribute[itemType].Name, after * 100 / PetLifeWarning::MaxLife);
    PostSystem(line, mu::ui::window::TYPE_ERROR_MESSAGE);
    PlayBuffer(SOUND_ERROR01);
}

void NoteEquipmentDurability(int slot, int before, int after)
{
    if (slot == EQUIPMENT_HELPER)
        NotePetLife(before, after);

    if (slot < 0 || slot >= MAX_EQUIPMENT || before <= 0 || after != 0)
        return;
    const int itemType = CharacterMachine->Equipment[slot].Type;
    if (itemType < 0 || IsAmmunition(itemType))
        return;

    wchar_t line[192]{};
    mu_swprintf(line, I18N::Game::EquipmentBroken, ItemAttribute[itemType].Name);
    PostSystem(line, mu::ui::window::TYPE_ERROR_MESSAGE);
    PlayBuffer(SOUND_ERROR01);
}

void Reset()
{
    g_buffEnds.Clear();
    g_bagRoom = BagRoomWarning{};
    g_petLife.Reset();
    g_petLifeType = -1;
}

void Tick()
{
    const std::uint32_t now = GetTickCount();
    TrackWorld(now);
    // A death takes every buff off: nothing to say about them.
    if (Hero != nullptr && Hero->Dead != 0)
    {
        g_buffEnds.Clear();
        return;
    }
    const std::vector<int> due = g_buffEnds.TakeDue(now);
    if (!due.empty())
        PostBuffsEnded(due);
}
} // namespace UI::Chat::StatusLines
