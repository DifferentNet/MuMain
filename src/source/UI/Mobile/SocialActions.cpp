#include "stdafx.h"
#include "UI/Mobile/SocialActions.h"

#include "Core/Input/TouchInput.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Automation/Movement.h"
#include "Guild/GuildCache.h"
#include "Guild/GuildTypes.h"
#include "I18N/All.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/CommandWindow.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace
{
using UI::Mobile::Social::Action;
using Clock = std::chrono::steady_clock;

// The command window's reach: the request goes out within this many tiles each way.
constexpr int CommandReach = MAX_DISTANCE_TILE;
// A walk to a player gives up after this long.
constexpr auto WalkLimit = std::chrono::seconds(10);
// How often the walk is planned again while the player moves: the server reads a walk packet every
// frame as a speed hack.
constexpr auto WalkRepeat = std::chrono::milliseconds(400);

struct Pending
{
    bool active = false;
    Action action = Action::Trade;
    int key = -1;
    Clock::time_point until{};
    Clock::time_point nextWalk{};
};
Pending g_pending;

bool InWorld()
{
    return Hero != nullptr && Hero->Object.Live && Hero->Dead == 0;
}

CHARACTER* FindByKey(int key)
{
    if (key < 0)
        return nullptr;
    const int index = FindCharacterIndex(key);
    return index >= 0 && index < MAX_CHARACTERS_CLIENT ? &CharactersClient[index] : nullptr;
}

// Another player, as the command window takes one: not the character, not a Christmas figure.
bool IsOtherPlayer(const CHARACTER* character)
{
    if (character == nullptr || character == Hero || !character->Object.Live)
        return false;
    if (character->Object.Kind != KIND_PLAYER || (character->Object.Type != MODEL_PLAYER && !character->Change))
        return false;
    const int subType = character->Object.SubType;
    return subType != MODEL_XMAS_EVENT_CHA_DEER && subType != MODEL_XMAS_EVENT_CHA_SNOWMAN &&
           subType != MODEL_XMAS_EVENT_CHA_SSANTA;
}

// Tiles to the player, the larger of the two ways (the command window's measure).
int TilesTo(const CHARACTER* character)
{
    return std::max(std::abs(character->PositionX - Hero->PositionX), std::abs(character->PositionY - Hero->PositionY));
}

bool NeedsReach(Action action)
{
    // A whisper and a friend request go by name, and follow walks after the player by itself.
    return action != Action::Whisper && action != Action::Friend && action != Action::Follow;
}

void Fire(Action action, CHARACTER* player)
{
    auto* commands = g_pNewUISystem->GetUI_NewCommandWindow();
    if (commands == nullptr)
        return;
    switch (action)
    {
    case Action::Trade:
        commands->CommandTrade(player);
        break;
    case Action::Party:
        commands->CommandParty(player->Key);
        break;
    case Action::Whisper:
        commands->CommandWhisper(player);
        // The chat line opens with the name in its whisper field.
        Core::Input::Touch::TapKey(VK_RETURN);
        break;
    case Action::Friend:
        commands->CommandAddFriend(player);
        break;
    case Action::Duel:
        commands->CommandDual(player);
        break;
    case Action::Follow:
        commands->CommandFollow(FindCharacterIndex(player->Key));
        break;
    case Action::Guild:
        commands->CommandGuild(player);
        break;
    case Action::Alliance:
        commands->CommandGuildUnion(player);
        break;
    case Action::Rival:
        commands->CommandGuildRival(player);
        break;
    case Action::EndRivalry:
        commands->CommandCancelGuildRival(player);
        break;
    case Action::Count:
        break;
    }
}

void SayOutOfReach(const CHARACTER* player)
{
    wchar_t text[128];
    mu_swprintf_s(text, I18N::Game::TouchSocialOutOfReach, player != nullptr ? player->ID : L"");
    g_pSystemLogBox->AddText(text, mu::ui::window::TYPE_ERROR_MESSAGE);
}
} // namespace

namespace UI::Mobile::Social
{
const wchar_t* Label(Action action)
{
    switch (action)
    {
    case Action::Trade:
        return I18N::Game::Trade;
    case Action::Party:
        return I18N::Game::Party;
    case Action::Whisper:
        return I18N::Game::Whisper;
    case Action::Friend:
        return I18N::Game::AddFriend;
    case Action::Duel:
        return I18N::Game::Duel;
    case Action::Follow:
        return I18N::Game::Follow;
    case Action::Guild:
        return I18N::Game::TouchSocialJoinGuild;
    case Action::Alliance:
        return I18N::Game::Alliance;
    case Action::Rival:
        return I18N::Game::HostilityGuild;
    case Action::EndRivalry:
        return I18N::Game::SuspendHostilities;
    case Action::Count:
        break;
    }
    return L"";
}

bool Offered(Action action)
{
    if (Hero == nullptr)
        return false;
    switch (action)
    {
    case Action::Guild:
        return Hero->GuildStatus == G_NONE;
    case Action::Alliance:
    case Action::Rival:
    case Action::EndRivalry:
        return Hero->GuildStatus == G_MASTER;
    default:
        return true;
    }
}

std::vector<Player> NearbyPlayers(int count)
{
    std::vector<Player> players;
    if (!InWorld())
        return players;
    struct Found
    {
        int tiles;
        const CHARACTER* character;
    };
    std::vector<Found> found;
    for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
    {
        const CHARACTER* character = &CharactersClient[index];
        if (!IsOtherPlayer(character))
            continue;
        const int tiles = TilesTo(character);
        if (tiles <= ListRange)
            found.push_back({tiles, character});
    }
    std::stable_sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.tiles < b.tiles; });
    for (const Found& one : found)
    {
        if (static_cast<int>(players.size()) >= count)
            break;
        Player player;
        player.key = one.character->Key;
        player.name = one.character->ID;
        const int mark = one.character->GuildMarkIndex;
        if (mark >= 0 && mark < MAX_MARKS)
            player.guild = GuildMark[mark].GuildName;
        players.push_back(std::move(player));
    }
    return players;
}

bool IsAround(int key)
{
    const CHARACTER* character = FindByKey(key);
    return InWorld() && IsOtherPlayer(character) && TilesTo(character) <= ListRange;
}

int AimedPlayer()
{
    for (const int index : {SelectedCharacter, Attacking != -1 ? ActionTarget : -1})
    {
        if (index < 0 || index >= MAX_CHARACTERS_CLIENT)
            continue;
        const CHARACTER* character = &CharactersClient[index];
        if (IsOtherPlayer(character) && IsAround(character->Key))
            return character->Key;
    }
    return -1;
}

bool RefusedHere()
{
    if (!gMapManager.InChaosCastle() && !::IsStrifeMap(gMapManager.WorldActive))
        return false;
    g_pSystemLogBox->AddText(I18N::Game::TheCommandWindowCannotBeActivatedInBattleZone,
                             mu::ui::window::TYPE_SYSTEM_MESSAGE);
    return true;
}

void Start(Action action, int key)
{
    CHARACTER* player = FindByKey(key);
    if (!InWorld() || !IsOtherPlayer(player))
        return;
    if (!NeedsReach(action) || TilesTo(player) <= CommandReach)
    {
        g_pending.active = false;
        Fire(action, player);
        return;
    }
    g_pending = Pending{true, action, key, Clock::now() + WalkLimit, Clock::time_point{}};
}

void Update(bool steering)
{
    if (!g_pending.active)
        return;
    if (steering || !InWorld())
    {
        g_pending.active = false;
        return;
    }
    CHARACTER* player = FindByKey(g_pending.key);
    const auto now = Clock::now();
    if (!IsOtherPlayer(player) || now >= g_pending.until)
    {
        g_pending.active = false;
        SayOutOfReach(player);
        return;
    }
    if (TilesTo(player) <= CommandReach)
    {
        g_pending.active = false;
        GameLogic::Automation::StopAtNextTile();
        Fire(g_pending.action, player);
        return;
    }
    if (now < g_pending.nextWalk)
        return;
    g_pending.nextWalk = now + WalkRepeat;
    if (GameLogic::Automation::WalkTo(player->PositionX, player->PositionY) == GameLogic::Automation::MoveResult::NoPath)
    {
        g_pending.active = false;
        SayOutOfReach(player);
    }
}

void Cancel()
{
    g_pending.active = false;
}
} // namespace UI::Mobile::Social
