#include "stdafx.h"
#include "GameLogic/Touch/TouchActions.h"

#include "Audio/DSPlaySound.h"
#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzOpenData.h"
#include "GameLogic/Automation/Attack.h"
#include "GameLogic/Automation/Movement.h"
#include "GameLogic/Automation/Pickup.h"
#include "GameLogic/Automation/Skill.h"
#include "GameLogic/Combat/DuelMgr.h"
#include "GameLogic/Combat/SkillExecution.h"
#include "GameLogic/Pets/GIPetManager.h"
#include "GameLogic/Skills/SkillManager.h"
#include "GameLogic/Social/PartyManager.h"
#include "GameLogic/Touch/SteerHold.h"
#include "I18N/All.h"
#include "MUHelper/MuHelper.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/QuickCommandWindow.h"
#include "World/GameMaps/GMBattleCastle.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// Defined in ZzzInterface.cpp: the character under the pointer, which a tap on a monster leaves there.
extern int SelectedCharacter;
// Defined in WSclient.cpp: a guild war is on.
extern bool EnableGuildWar;

namespace
{
// The keyboard's ten skill hotkeys, 1-9 and 0 (CSkillList keeps its count to itself).
constexpr int HotKeyCount = 10;
} // namespace

namespace GameLogic::Touch
{
namespace
{
using Clock = std::chrono::steady_clock;

// The pace of the walk packets and of the other actions: the control socket's and the MU Helper's.
constexpr auto WalkRepeat = std::chrono::milliseconds(300);
// A turn of the joystick, or a fresh push, sends its walk sooner: at 300 ms a turn lagged behind the
// thumb. The server charges a walk the tiles walked, not the packets (the walk budget).
constexpr auto TurnRepeat = std::chrono::milliseconds(120);
constexpr auto ActionRepeat = std::chrono::milliseconds(250);
// How long a monster that could not be fought (behind a wall, out of reach) is passed over.
constexpr auto ForgetTime = std::chrono::seconds(3);
// How long a cast where the character stands holds the joystick's walk off while it stops, and how
// soon it may stop the walk again when that cast still did not go off.
constexpr auto StandTime = std::chrono::milliseconds(600);
constexpr auto StandAgainTime = std::chrono::milliseconds(1500);
// How long the joystick waits after a swing went off, before the swing's own pose holds it.
constexpr auto SwingStartTime = std::chrono::milliseconds(200);
// How long a drop asked for is passed over while the server answers, and one no walk reaches.
constexpr auto AskedDropTime = std::chrono::milliseconds(1500);
constexpr auto UnreachableDropTime = std::chrono::seconds(3);
// A drop still lying there after this many asks is not ours to take (another's, a refusal): it is
// passed over this long instead.
constexpr int RefusedDropAsks = 2;
constexpr auto RefusedDropTime = std::chrono::seconds(30);
// How often the pickup button says the bag is full.
constexpr auto FullBagRepeat = std::chrono::seconds(3);
// How often a skill button short of mana or AG asks for a mana potion and says so.
constexpr auto ShortOfManaRepeat = std::chrono::milliseconds(1500);
// How often it says so in the chat, while held: a line every 1.5 s buried the chat.
constexpr auto ShortOfManaSayRepeat = std::chrono::seconds(6);
// How often a held pet command button sends its command again.
constexpr auto PetCommandRepeat = std::chrono::seconds(1);
// How often the pad's Teleport Ally with no friend chosen says so.
constexpr auto NoFriendSayRepeat = std::chrono::seconds(3);
// A Nova held this long goes off by itself: the server's charge is full well before.
constexpr auto LongestCharge = std::chrono::seconds(5);
// The pad's Teleport jumps at least this many tiles, else it does not go off.
constexpr int ShortestTeleport = 2;

// How far ahead the joystick walks, in tiles, and the shorter walk tried along a wall.
constexpr int StickTiles = 4;
constexpr int ShortStickTiles = 2;
// The joystick turned this far (radians) from the walk under way: a new walk at once.
constexpr float TurnToReplan = 0.35f;
// A walk with this many steps left is extended before the character stops.
constexpr int StepsLeftToReplan = 2;

// Monsters within this many tiles are fought by the attack and skill buttons.
constexpr int TargetRange = 8;
// A chosen party member this near is in sight: a heal or Teleport Ally walks up to them.
constexpr int FriendSightRange = 14;
// How far beyond its reach the path finder may look for a way to the target.
constexpr int HuntingDistance = 8;
// Drops within this many tiles are gathered by the pickup button.
constexpr int PickupRange = 6;
// How far from the character Hellfire's and Inferno's fire reaches, in tiles: cast where the
// character stands, they hit nothing farther away.
constexpr float FireRingTiles = 3.f;

constexpr int NoTarget = -1;
constexpr int NoDrop = -1;
constexpr int NoHotKey = -1;

// The Nova charging (the server grows its damage until it is let go, and a Nova sent without its
// start does nothing): its hotkey, and when it began.
int g_chargeHotKey = NoHotKey;
Clock::time_point g_chargeStart{};

struct Steering
{
    // A walk of the joystick's is under way.
    bool active = false;
    // The direction of the walk under way, on the map (tiles).
    float dirX = 0.f;
    float dirY = 0.f;
    Clock::time_point lastWalk;
    // Where the character was when the walk was sent.
    HeroPlace from;
};

Steering g_steering;
SteerHold g_hold;
// Until when a button's cast holds the joystick's walk off, and before when it may not stop the
// walk again.
Clock::time_point g_standUntil;
Clock::time_point g_standAgainAt;
bool g_pointerAims = false;
bool g_padPlaysWorld = false;
// The pad's left stick now, in screen directions; 0, 0 while it rests (UI/HUD/GamepadControls).
float g_padAimX = 0.f;
float g_padAimY = 0.f;
int g_targetKey = NoTarget;
// The target the pad's right stick chose (CycleTarget): the buttons' target while it is a good one.
int g_chosenKey = NoTarget;
// The NPC or player the right stick chose where no foe is near (MeetIndex).
int g_meetKey = NoTarget;
// When the walk up to a player for their menu was last sent (WalkUpTo).
Clock::time_point g_walkUpAt;
std::map<int, Clock::time_point> g_forgottenUntil;
Clock::time_point g_nextAction;
// g_nextAction before the last ActionDue() took the turn (GiveTurnBack()).
Clock::time_point g_turnTakenFrom;
// The drops passed over (asked for, or out of a walk's reach), by their slot: until when, and which
// drop it was (a slot is used again for another drop) and how often it was asked for.
struct SkippedDrop
{
    Clock::time_point until;
    int type = 0;
    int tileX = 0;
    int tileY = 0;
    int asks = 0;
};
std::map<int, SkippedDrop> g_skippedDrops;
// The drop the pickup button walks to, and when the bag was last said to be full.
int g_dropWalkedTo = NoDrop;
Clock::time_point g_fullBagSaidAt;
// The PK switch (SetForceAttack) and the party member the heals and buffs go to (ToggleFriend).
bool g_forceAttack = false;
int g_friendKey = NoTarget;
// The chosen member's name, for the message when they are out of sight.
std::wstring g_friendName;
// When a skill short of mana or AG may ask and say so again, and when a held pet command may go out
// again.
Clock::time_point g_shortOfManaAt;
Clock::time_point g_shortOfManaSaidAt;
Clock::time_point g_petCommandAt;
Clock::time_point g_noFriendSaidAt;

bool InWorld()
{
    return Hero != nullptr && Hero->Object.Live && Hero->Dead == 0;
}

// An action may go out: the last one went out long enough ago. Asking spends the turn: an attempt
// that was refused may have sent a packet of its own (a mana potion the cast asked for).
bool ActionDue()
{
    const auto now = Clock::now();
    if (now < g_nextAction)
        return false;
    g_turnTakenFrom = g_nextAction;
    g_nextAction = now + ActionRepeat;
    return true;
}

// An action that took the turn but found it could not go off yet, with nothing sent: another button
// held may have the turn.
Progress GiveTurnBack()
{
    g_nextAction = g_turnTakenFrom;
    return Progress::Waiting;
}

// An action that took the turn and found nothing to do (no monster, no drop): it is over, and the
// turn goes to another button held.
Progress NothingToDo()
{
    g_nextAction = g_turnTakenFrom;
    return Progress::Done;
}

double Seconds(Clock::time_point time)
{
    return std::chrono::duration<double>(time.time_since_epoch()).count();
}

HeroPlace HerePlace()
{
    return {static_cast<int>(gMapManager.WorldActive), Hero->PositionX, Hero->PositionY, Seconds(Clock::now())};
}

// Whether the character stands within `range` tiles of the tile's centre, measured as the engine's
// CheckTile measures it.
bool HeroWithin(int tileX, int tileY, float range)
{
    const float dx = Hero->Object.Position[0] - (static_cast<float>(tileX) + 0.5f) * TERRAIN_SCALE;
    const float dy = Hero->Object.Position[1] - (static_cast<float>(tileY) + 0.5f) * TERRAIN_SCALE;
    const float reach = range * TERRAIN_SCALE;
    return dx * dx + dy * dy <= reach * reach;
}

int TileOf(float position)
{
    return static_cast<int>(position / TERRAIN_SCALE);
}

// The walk under way is forgotten where the client keeps it, with nothing sent: the server put the
// character elsewhere (a gate, a map change), where a stop on the old walk's tiles means nothing.
void ForgetWalk()
{
    {
        std::lock_guard<SpinLock> guard(Hero->Path.Lock);
        Hero->Path.PathNum = 0;
        Hero->Path.CurrentPath = 0;
        Hero->Path.CurrentPathFloat = 0;
    }
    if (Hero->Movement)
    {
        Hero->Movement = false;
        SetPlayerStop(Hero);
    }
}

// Ends the joystick's walk: on the same ground the character finishes its step and the server is
// told where it stands; past a gate the walk is only forgotten.
void EndWalk()
{
    if (!g_steering.active)
        return;
    g_steering.active = false;
    const HeroPlace here = HerePlace();
    if (here.world == g_steering.from.world && !IsJump(g_steering.from, here))
        // Clearing the path alone left it walking straight on for good (a Pixel 9 Pro XL, 2026-10-30).
        Automation::StopAtNextTile();
    else
        ForgetWalk();
}

// The joystick's direction on the map: the screen directions of the map's two axes at the
// character, from the camera, and the joystick's direction written in them.
bool ScreenToMap(float screenX, float screenY, float& mapX, float& mapY)
{
    vec3_t origin;
    vec3_t alongX;
    vec3_t alongY;
    VectorCopy(Hero->Object.Position, origin);
    VectorCopy(origin, alongX);
    VectorCopy(origin, alongY);
    alongX[0] += TERRAIN_SCALE;
    alongY[1] += TERRAIN_SCALE;

    float originX = 0.f, originY = 0.f, xAxisX = 0.f, xAxisY = 0.f, yAxisX = 0.f, yAxisY = 0.f;
    if (!CameraProjection::WorldToWindowPixel(g_Camera, origin, &originX, &originY) ||
        !CameraProjection::WorldToWindowPixel(g_Camera, alongX, &xAxisX, &xAxisY) ||
        !CameraProjection::WorldToWindowPixel(g_Camera, alongY, &yAxisX, &yAxisY))
        return false;

    const float ax = xAxisX - originX;
    const float ay = xAxisY - originY;
    const float bx = yAxisX - originX;
    const float by = yAxisY - originY;
    const float determinant = ax * by - ay * bx;
    if (std::fabs(determinant) < 1e-4f)
        return false;

    const float x = (screenX * by - screenY * bx) / determinant;
    const float y = (ax * screenY - ay * screenX) / determinant;
    const float length = std::hypot(x, y);
    if (length < 1e-4f)
        return false;
    mapX = x / length;
    mapY = y / length;
    return true;
}

// Steps left of the walk under way; none once the character stands, whatever path it kept.
int StepsLeft()
{
    if (!Hero->Movement)
        return 0;
    return Hero->Path.PathNum > 0 ? Hero->Path.PathNum - 1 - Hero->Path.CurrentPath : 0;
}

// A walk the way the joystick points, or a little to its side when a wall stands right ahead.
bool WalkToward(float dirX, float dirY)
{
    constexpr std::array<float, 5> Turns = {0.f, 0.52f, -0.52f, 1.05f, -1.05f};
    for (const int tiles : {StickTiles, ShortStickTiles})
    {
        for (const float turn : Turns)
        {
            const float c = std::cos(turn);
            const float s = std::sin(turn);
            const float x = dirX * c - dirY * s;
            const float y = dirX * s + dirY * c;
            const int tileX = Hero->PositionX + static_cast<int>(std::lround(x * tiles));
            const int tileY = Hero->PositionY + static_cast<int>(std::lround(y * tiles));
            if (tileX < 0 || tileY < 0 || tileX >= TERRAIN_SIZE || tileY >= TERRAIN_SIZE)
                continue;
            if (Automation::WalkTo(tileX, tileY) == Automation::MoveResult::Walking)
                return true;
        }
    }
    return false;
}

// The joystick walks away from the fight: the engine's own auto-attack (Attacking, kept by a tap on
// a monster or a button's swing) would chase the monster again against it, and the next press of
// the attack button looks for the nearest monster again.
void BreakOffFight()
{
    Attacking = -1;
    SelectedCharacter = -1;
    g_targetKey = NoTarget;
    g_dropWalkedTo = NoDrop;
}

// A button's swing or cast holds the joystick's walk: the swing plays to its end (a walk would cut
// it short), and a cast where the character stands waits for it to stop.
bool ButtonHoldsHero()
{
    return Automation::IsSwingInProgress() || Clock::now() < g_standUntil;
}

// Whether the character stands, so a cast where it stands can go off (GameLogic::Combat::ExecuteSkill
// casts from a standing pose only). A walk under way is ended for it, at most once in a while: a
// cast that still does not go off then lets the walk run on.
bool HeroStands()
{
    if (!Hero->Movement)
        return true;
    const auto now = Clock::now();
    if (now < g_standUntil || now < g_standAgainAt)
        return false;
    g_standUntil = now + StandTime;
    g_standAgainAt = now + StandAgainTime;
    g_steering.active = false;
    Automation::StopAtNextTile();
    return false;
}

// A swing or a cast went off: the next cast may stop the walk at once, and the joystick waits for
// the swing to take its pose.
void ActionWentOff()
{
    const auto now = Clock::now();
    g_standAgainAt = now;
    g_standUntil = now + SwingStartTime;
}

int IndexOf(const CHARACTER* character)
{
    return static_cast<int>(character - CharactersClient);
}

// The places where the game itself sets players against each other: a duel, Chaos Castle, a Gens
// fight, a guild war, the castle siege, the Illusion Temple. Elsewhere a player is fought only when
// aimed at (an outlaw, a rival guild's member, anyone with the PK switch on).
bool PlayersFightHere()
{
    return g_DuelMgr.IsDuelEnabled() || gMapManager.InChaosCastle() || IsStrifeMap(gMapManager.WorldActive) ||
           EnableGuildWar || (gMapManager.InBattleCastle() && battleCastle::IsBattleCastleStart()) ||
           gMapManager.IsCursedTemple();
}

// Whether the buttons may fight the character, as a click may (CanAttackCharacter): a monster, or a
// player who is an enemy here. A player the player did not aim at only where players fight, and never
// by the PK switch alone; never a member of the party.
bool IsFoe(const CHARACTER* character, bool aimed)
{
    if (character == nullptr || !character->Object.Live || character->Dead != 0 || character == Hero)
        return false;
    const int index = IndexOf(character);
    if (IsMonster(const_cast<CHARACTER*>(character)))
        return CanAttackCharacter(index, false);
    if (character->Object.Kind != KIND_PLAYER || g_pPartyManager->IsPartyMember(index))
        return false;
    if (!aimed && !PlayersFightHere())
        return false;
    return CanAttackCharacter(index, aimed && g_forceAttack);
}

// Tiles to the monster, where it stands or where it is walking to, whichever is nearer: as the MU
// Helper measures it.
int TilesTo(const CHARACTER* character)
{
    const auto distance = [](int dx, int dy) { return static_cast<int>(std::ceil(std::hypot(dx, dy))); };
    return std::min(distance(Hero->PositionX - character->PositionX, Hero->PositionY - character->PositionY),
                    distance(Hero->PositionX - character->TargetX, Hero->PositionY - character->TargetY));
}

// Whether the monster stands within `range` tiles, as the attack and the cast measure it.
bool InReach(const CHARACTER* character, float range)
{
    return HeroWithin(TileOf(character->Object.Position[0]), TileOf(character->Object.Position[1]), range);
}

CHARACTER* FindByKey(int key)
{
    if (key == NoTarget)
        return nullptr;
    const int index = FindCharacterIndex(key);
    return index >= 0 && index < MAX_CHARACTERS_CLIENT ? &CharactersClient[index] : nullptr;
}

bool IsForgotten(int key)
{
    const auto it = g_forgottenUntil.find(key);
    if (it == g_forgottenUntil.end())
        return false;
    if (Clock::now() < it->second)
        return true;
    g_forgottenUntil.erase(it);
    return false;
}

bool IsGoodTarget(const CHARACTER* character, bool aimed)
{
    return IsFoe(character, aimed) && !IsForgotten(character->Key) && TilesTo(character) <= TargetRange;
}

// An NPC or another player near, for the right stick to choose where no foe is near.
bool IsGoodMeet(const CHARACTER* character)
{
    if (character == nullptr || character == Hero || !character->Object.Live || character->Dead != 0)
        return false;
    if (character->Object.Kind != KIND_NPC && character->Object.Kind != KIND_PLAYER)
        return false;
    // A player whose menu may not open here (a temple teammate, while watching a duel) is not offered:
    // A would promise the menu and do nothing.
    if (character->Object.Kind == KIND_PLAYER && !mu::ui::window::CQuickCommandWindow::IsMenuAllowedFor(character))
        return false;
    if (TilesTo(character) > TargetRange)
        return false;
    // Only one on view: one past the screen's edge would be chosen unseen, its name and
    // prompt nowhere.
    float x = 0.f;
    float y = 0.f;
    return CameraProjection::WorldToWindowPixel(g_Camera, character->Object.Position, &x, &y) && x >= 0.f
           && y >= 0.f && x < static_cast<float>(WindowWidth) && y < static_cast<float>(WindowHeight);
}

// The next key in a list sorted by (tiles, key) after `current`, step 1 the next further, -1 the next
// nearer, going round; the nearest (or the furthest for -1) when `current` is not in it.
int NextInRound(const std::vector<std::pair<int, int>>& keys, int current, int step)
{
    const auto found = std::find_if(keys.begin(), keys.end(), [current](const auto& key) { return key.second == current; });
    const int count = static_cast<int>(keys.size());
    int next = 0;
    if (found == keys.end())
        next = step >= 0 ? 0 : count - 1;
    else
        next = ((static_cast<int>(found - keys.begin()) + step) % count + count) % count;
    return keys[next].second;
}

// The character the player aims at: the one the pad's pointer is on, else the one the engine fights
// (a tap on it, or the buttons' own attack: ActionTarget). A finger's pointer stays where it last
// tapped, so a character that walks under it later is no aim.
CHARACTER* AimedCharacter()
{
    int index = -1;
    if (g_pointerAims && SelectedCharacter >= 0)
        index = SelectedCharacter;
    else if (Attacking != -1)
        index = ActionTarget;
    if (index < 0 || index >= MAX_CHARACTERS_CLIENT)
        return nullptr;
    return &CharactersClient[index];
}

// The monster or player aimed at, else the one fought last (a player aimed at once stays the target),
// else the nearest monster, or the nearest enemy player where players fight.
int ChooseTarget()
{
    if (g_chosenKey != NoTarget)
    {
        if (IsGoodTarget(FindByKey(g_chosenKey), true))
            return g_targetKey = g_chosenKey;
        g_chosenKey = NoTarget;
    }
    const CHARACTER* aimed = AimedCharacter();
    if (IsGoodTarget(aimed, true))
        return g_targetKey = aimed->Key;

    if (IsGoodTarget(FindByKey(g_targetKey), true))
        return g_targetKey;

    int nearestKey = NoTarget;
    int nearestTiles = TargetRange + 1;
    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        const CHARACTER* character = &CharactersClient[i];
        if (!IsGoodTarget(character, false))
            continue;
        const int tiles = TilesTo(character);
        if (tiles < nearestTiles)
        {
            nearestTiles = tiles;
            nearestKey = character->Key;
        }
    }
    return g_targetKey = nearestKey;
}

void Forget(int key)
{
    g_forgottenUntil[key] = Clock::now() + ForgetTime;
    if (g_targetKey == key)
        g_targetKey = NoTarget;
}

// Whether a skill helps the character instead of hurting a monster, and whether it is aimed at the
// character (the MU Helper's buff table, CMuHelper::BuffTarget, the heals and the summons).
enum class SkillAim
{
    Monster,
    Self,
    Around,
    // Aimed where the player points (a long press on the ground): the button only selects it.
    Pointer,
};

// The Elf's summons: Goblin, Stone Golem, Assassin, Elite Yeti, Dark Knight, Bali, Soldier.
constexpr int SummonCount = 7;

SkillAim AimOf(ActionSkillType skill)
{
    if (skill >= AT_SKILL_SUMMON && skill < AT_SKILL_SUMMON + SummonCount)
        return SkillAim::Around;
    switch (skill)
    {
    case AT_SKILL_ATTACK:
    case AT_SKILL_ATTACK_STR:
    case AT_SKILL_ATTACK_MASTERY:
    case AT_SKILL_DEFENSE:
    case AT_SKILL_DEFENSE_STR:
    case AT_SKILL_DEFENSE_MASTERY:
    case AT_SKILL_SOUL_BARRIER:
    case AT_SKILL_SOUL_BARRIER_STR:
    case AT_SKILL_SOUL_BARRIER_PROFICIENCY:
    case AT_SKILL_HEALING:
    case AT_SKILL_HEALING_STR:
    case AT_SKILL_RECOVER:
        return SkillAim::Self;
    case AT_SKILL_INFINITY_ARROW:
    case AT_SKILL_INFINITY_ARROW_STR:
    case AT_SKILL_SWELL_LIFE:
    case AT_SKILL_SWELL_LIFE_STR:
    case AT_SKILL_SWELL_LIFE_PROFICIENCY:
    case AT_SKILL_EXPANSION_OF_WIZARDRY:
    case AT_SKILL_EXPANSION_OF_WIZARDRY_STR:
    case AT_SKILL_EXPANSION_OF_WIZARDRY_MASTERY:
    case AT_SKILL_ADD_CRITICAL:
    case AT_SKILL_ADD_CRITICAL_STR1:
    case AT_SKILL_ADD_CRITICAL_STR2:
    case AT_SKILL_ADD_CRITICAL_STR3:
    case AT_SKILL_ALICE_BERSERKER:
    case AT_SKILL_ALICE_BERSERKER_STR:
    case AT_SKILL_ALICE_THORNS:
    case AT_SKILL_ATT_UP_OURFORCES:
    case AT_SKILL_HP_UP_OURFORCES:
    case AT_SKILL_HP_UP_OURFORCES_STR:
    case AT_SKILL_DEF_UP_OURFORCES:
    case AT_SKILL_DEF_UP_OURFORCES_STR:
    case AT_SKILL_DEF_UP_OURFORCES_MASTERY:
    case AT_SKILL_IMPROVE_AG:
    case AT_SKILL_PARTY_TELEPORT:
        return SkillAim::Around;
    case AT_SKILL_TELEPORT:
    case AT_SKILL_TELEPORT_ALLY:
        return SkillAim::Pointer;
    default:
        return SkillAim::Monster;
    }
}

// The heals and buffs a party member may take as well: a right click on them, or on their party
// card, casts these on them (CPartyListWindow::SelectCharacterInPartyList).
bool GoesOnFriend(ActionSkillType skill)
{
    switch (skill)
    {
    case AT_SKILL_HEALING:
    case AT_SKILL_HEALING_STR:
    case AT_SKILL_DEFENSE:
    case AT_SKILL_DEFENSE_STR:
    case AT_SKILL_DEFENSE_MASTERY:
    case AT_SKILL_ATTACK:
    case AT_SKILL_ATTACK_STR:
    case AT_SKILL_ATTACK_MASTERY:
    case AT_SKILL_SOUL_BARRIER:
    case AT_SKILL_SOUL_BARRIER_STR:
    case AT_SKILL_SOUL_BARRIER_PROFICIENCY:
    case AT_SKILL_ALICE_THORNS:
    case AT_SKILL_RECOVER:
        return true;
    default:
        return false;
    }
}

// A live party member other than the character, however far.
bool IsLiveMember(const CHARACTER* character)
{
    return character != nullptr && character != Hero && character->Object.Live && character->Dead == 0 &&
           character->Object.Kind == KIND_PLAYER && g_pPartyManager->IsPartyMember(IndexOf(character));
}

bool IsFriend(const CHARACTER* character)
{
    return IsLiveMember(character) && TilesTo(character) <= TargetRange;
}

// Forgets the chosen party member once they are out of the party (left it, or it broke up), on the
// screen or off it: the party list holds every member by name, wherever they are.
void ForgetGoneFriend()
{
    if (g_friendKey == NoTarget)
        return;
    for (int member = 0; member < PartyNumber; ++member)
    {
        if (!g_friendName.empty() && g_friendName == Party[member].Name)
            return;
    }
    g_friendKey = NoTarget;
    g_friendName.clear();
}

// The chosen party member on the screen, found by name: their object key changes with each map they
// enter, so the key is refreshed from what is found. None once they left the party.
const CHARACTER* ChosenMember()
{
    ForgetGoneFriend();
    if (g_friendKey == NoTarget)
        return nullptr;
    for (int index = 0; index < MAX_CHARACTERS_CLIENT; ++index)
    {
        const CHARACTER& character = CharactersClient[index];
        if (character.Object.Live && character.Object.Kind == KIND_PLAYER && &character != Hero &&
            g_friendName == character.ID)
        {
            g_friendKey = character.Key;
            return &character;
        }
    }
    return nullptr;
}

// The chosen party member while in sight, near or not: a heal walks up to them.
const CHARACTER* FriendInSight()
{
    const CHARACTER* chosen = ChosenMember();
    return IsLiveMember(chosen) && TilesTo(chosen) <= FriendSightRange ? chosen : nullptr;
}

// The party member a heal or a buff goes to: the one the pad's pointer is on, else the one RB or a tap
// on their party card chose, while in sight; none for the character itself.
const CHARACTER* ChosenFriend()
{
    if (g_pointerAims && SelectedCharacter >= 0 && SelectedCharacter < MAX_CHARACTERS_CLIENT &&
        IsFriend(&CharactersClient[SelectedCharacter]))
        return &CharactersClient[SelectedCharacter];
    return FriendInSight();
}

// A party member is chosen, still in the party, but gone from the screen (another map, far off) or
// farther than FriendSightRange.
bool FriendOutOfSight()
{
    const CHARACTER* chosen = ChosenMember();
    if (g_friendKey == NoTarget)
        return false;
    return chosen == nullptr || TilesTo(chosen) > FriendSightRange;
}

// A party member is chosen but out of sight: the player is told, every few seconds at most, rather than
// the heal going to the character or Teleport Ally asking for a choice already made.
// The turn goes back, so a held Attack keeps swinging.
Progress FriendTooFar(const std::wstring& name)
{
    const auto now = Clock::now();
    if (now >= g_noFriendSaidAt)
    {
        g_noFriendSaidAt = now + NoFriendSayRepeat;
        std::wstring text = I18N::Game::PadFriendTooFar;
        text += name;
        UI::Chat::PostSystem(text, mu::ui::window::TYPE_ERROR_MESSAGE);
    }
    return NothingToDo();
}

// Whether the character has the mana and the AG for the skill, as GameLogic::Combat::CheckMana weighs
// them, without the potion it asks for.
bool HasManaFor(ActionSkillType skill)
{
    int mana = 0;
    int skillMana = 0;
    gSkillManager.GetSkillInformation(skill, 1, nullptr, &mana, nullptr, &skillMana);
    return CharacterAttribute->Mana >= mana && CharacterAttribute->SkillMana >= skillMana;
}

// The skill-list index (or pet command) a hotkey holds for the character now, or -1.
int HotKeySkillIndex(int hotKey)
{
    if (!g_pSkillList->IsHotKeyShown(hotKey))
        return -1;
    const int index = g_pSkillList->GetHotKey(hotKey);
    if (index >= AT_PET_COMMAND_DEFAULT && index < AT_PET_COMMAND_END)
        return index;
    // A skill the character no longer has (another character's hotkeys, a skill taken back).
    return index >= 0 && index < MAX_SKILLS && CharacterAttribute->Skill[index] != 0 ? index : -1;
}

Progress OutcomeOf(Automation::SkillResult result)
{
    switch (result)
    {
    case Automation::SkillResult::Cast:
        ActionWentOff();
        return Progress::Done;
    case Automation::SkillResult::NoTarget:
    case Automation::SkillResult::Refused:
        return Progress::Done;
    default:
        // On its way, a swing still playing, not ready yet, or a target dropped for another.
        return Progress::Pending;
    }
}

// A cast where the character stands: a buff or a heal on itself, one around it, an area skill with
// no monster about. A cast that cannot go off yet (cooldown, mana) leaves the walk alone.
Progress CastWhereHeroStands(ActionSkillType skill, bool onSelf)
{
    if (!GameLogic::Combat::CanExecuteSkill(Hero, skill, gSkillManager.GetSkillDistance(skill, Hero)))
        return GiveTurnBack();
    if (!HeroStands())
        return Progress::Pending;
    return OutcomeOf(onSelf ? Automation::CastSkill(skill, true, Hero->Key, true, HuntingDistance)
                            : Automation::CastSkill(skill, false, NoTarget, true, HuntingDistance));
}

Progress CastAtMonster(ActionSkillType skill)
{
    const int target = ChooseTarget();
    if (target == NoTarget)
    {
        // No monster around: an area skill still goes off where the character stands, but not by
        // stopping the joystick's walk for nothing.
        return IsSteering() ? NothingToDo() : CastWhereHeroStands(skill, false);
    }

    const CHARACTER* targetCharacter = FindByKey(target);
    const bool fireRing = skill == AT_SKILL_HELL_FIRE || skill == AT_SKILL_HELL_FIRE_STR ||
                          skill == AT_SKILL_INFERNO || skill == AT_SKILL_INFERNO_STR || skill == AT_SKILL_INFERNO_STR_MG;
    // Hellfire and Inferno go off where the character stands: the monster has to be inside the
    // fire. Nova's charge takes it wherever the monster stands.
    const bool inReach = fireRing ? InReach(targetCharacter, FireRingTiles)
                                  : Automation::IsSelfPositionSkill(skill) ||
                                        InReach(targetCharacter, gSkillManager.GetSkillDistance(skill, Hero));
    // While the joystick walks, the buttons fight what is in reach: walking off towards a monster
    // would fight the joystick.
    if (!inReach && IsSteering())
        return GiveTurnBack();
    if (!inReach && fireRing)
    {
        // A cast where the character stands would walk nowhere: a walk towards the monster first. A
        // monster with no way to it (behind a wall) is left for another.
        if (Automation::WalkTo(TileOf(targetCharacter->Object.Position[0]), TileOf(targetCharacter->Object.Position[1])) ==
            Automation::MoveResult::NoPath)
        {
            Forget(target);
            return Progress::Done;
        }
        return Progress::Pending;
    }
    if (inReach)
    {
        if (!GameLogic::Combat::CanExecuteSkill(Hero, skill, gSkillManager.GetSkillDistance(skill, Hero)))
            return GiveTurnBack();
        if (!HeroStands())
            return Progress::Pending;
    }

    const Automation::SkillResult result = Automation::CastSkill(skill, true, target, true, HuntingDistance);
    if (Automation::ShouldForgetTarget(result) && result != Automation::SkillResult::Refused)
        Forget(target);
    return OutcomeOf(result);
}

// A heal or a buff on a party member, walking up to them when out of reach as a cast at a monster
// does.
Progress CastAtFriend(ActionSkillType skill, const CHARACTER* friendCharacter)
{
    const float distance = gSkillManager.GetSkillDistance(skill, Hero);
    const bool inReach = InReach(friendCharacter, distance);
    if (!inReach && IsSteering())
        return GiveTurnBack();
    if (inReach)
    {
        if (!GameLogic::Combat::CanExecuteSkill(Hero, skill, distance))
            return GiveTurnBack();
        if (!HeroStands())
            return Progress::Pending;
    }
    const Automation::SkillResult result =
        Automation::CastSkill(skill, true, friendCharacter->Key, true, HuntingDistance);
    // No way to them (round a wall, across water) or gone from the view: said, rather than the press
    // waiting out its follow-through for nothing.
    if (result == Automation::SkillResult::NoPath || result == Automation::SkillResult::NotInView)
        return FriendTooFar(friendCharacter->ID);
    return OutcomeOf(result);
}

// A tile Teleport may land on: what a mouse click there allows (AttackWizard), no wall, no safe zone,
// no water's edge.
bool IsTeleportTile(int tileX, int tileY)
{
    if (tileX < 0 || tileY < 0 || tileX >= TERRAIN_SIZE || tileY >= TERRAIN_SIZE)
        return false;
    const int wall = TerrainWall[TERRAIN_INDEX(tileX, tileY)] & ~(TW_ACTION | TW_HEIGHT);
    return wall == 0;
}

// Where the pad's Teleport lands: the farthest free tile within the skill's reach the way the left
// stick points now (also where no walk goes: across water, over a fence), or with the stick at rest
// the way the character last walked, or up the screen before any walk.
bool PadTeleportTile(ActionSkillType skill, int& tileX, int& tileY)
{
    float dirX = g_steering.dirX;
    float dirY = g_steering.dirY;
    if (g_padAimX != 0.f || g_padAimY != 0.f)
    {
        if (!ScreenToMap(g_padAimX, g_padAimY, dirX, dirY))
            return false;
    }
    else if (dirX == 0.f && dirY == 0.f && !ScreenToMap(0.f, -1.f, dirX, dirY))
        return false;
    const float reach = gSkillManager.GetSkillDistance(skill, Hero);
    for (int tiles = static_cast<int>(reach); tiles >= ShortestTeleport; --tiles)
    {
        const int dx = static_cast<int>(std::lround(dirX * tiles));
        const int dy = static_cast<int>(std::lround(dirY * tiles));
        if (std::hypot(static_cast<float>(dx), static_cast<float>(dy)) > reach)
            continue;
        if (IsTeleportTile(Hero->PositionX + dx, Hero->PositionY + dy))
        {
            tileX = Hero->PositionX + dx;
            tileY = Hero->PositionY + dy;
            return true;
        }
    }
    return false;
}

// The pad's Teleport (ghinzuro, 2026-11-04): the character stands, then jumps (PadTeleportTile).
Progress PadTeleport(ActionSkillType skill)
{
    if (!GameLogic::Combat::CanExecuteSkill(Hero, skill, gSkillManager.GetSkillDistance(skill, Hero)))
        return GiveTurnBack();
    if (!HeroStands())
        return Progress::Pending;
    int tileX = 0;
    int tileY = 0;
    if (!PadTeleportTile(skill, tileX, tileY))
        return Progress::Done;
    // One jump a press: the engine counts a teleport as no cast gone off, and a tap owed until then
    // jumped on and on. A held button jumps again as the skill's delay allows.
    const Automation::SkillResult result = Automation::CastSkillAtTile(skill, tileX, tileY);
    if (result == Automation::SkillResult::Busy)
        return Progress::Pending;
    if (result == Automation::SkillResult::Cast)
        ActionWentOff();
    // Away from the fight, as a click on the ground: the engine's auto-attack would walk back to the
    // monster chosen before.
    if (result != Automation::SkillResult::Refused)
        BreakOffFight();
    return Progress::Done;
}

// The pad's Teleport Ally: brings the friend RB chose to the character; with none, says how to choose.
Progress PadTeleportAlly(ActionSkillType skill)
{
    const CHARACTER* friendCharacter = ChosenFriend();
    if (friendCharacter != nullptr)
    {
        // Walks up to the friend first; in reach, one summon a press: the engine counts it as no cast,
        // and a tap owed until then pulled the friend again and again.
        const float distance = gSkillManager.GetSkillDistance(skill, Hero);
        if (!InReach(friendCharacter, distance))
            return CastAtFriend(skill, friendCharacter);
        if (!GameLogic::Combat::CanExecuteSkill(Hero, skill, distance))
            return GiveTurnBack();
        if (!HeroStands())
            return Progress::Pending;
        const Automation::SkillResult result =
            Automation::CastSkill(skill, true, friendCharacter->Key, true, HuntingDistance);
        if (result == Automation::SkillResult::Busy)
            return Progress::Pending;
        if (result == Automation::SkillResult::Cast || result == Automation::SkillResult::NotReady)
            ActionWentOff();
        return Progress::Done;
    }
    if (FriendOutOfSight())
        return FriendTooFar(g_friendName);
    const auto now = Clock::now();
    if (now >= g_noFriendSaidAt)
    {
        g_noFriendSaidAt = now + NoFriendSayRepeat;
        UI::Chat::PostSystem(I18N::Game::PadChooseFriendFirst, mu::ui::window::TYPE_ERROR_MESSAGE);
    }
    return Progress::Done;
}

// A skill the character lacks the mana or the AG for: once in a while the cast is tried, which asks
// for a mana potion (GameLogic::Combat::CheckMana), and the player is told. The action's turn stays
// the attack button's, and a tap is done: a tap again casts once the mana is back.
Progress ShortOfMana(ActionSkillType skill)
{
    const auto now = Clock::now();
    if (now < g_shortOfManaAt)
        return Progress::Done;
    g_shortOfManaAt = now + ShortOfManaRepeat;
    int mana = 0;
    gSkillManager.GetSkillInformation(skill, 1, nullptr, &mana, nullptr);
    const bool manaShort = CharacterAttribute->Mana < mana;
    GameLogic::Combat::CheckMana(Hero, skill);
    if (now >= g_shortOfManaSaidAt)
    {
        g_shortOfManaSaidAt = now + ShortOfManaSayRepeat;
        UI::Chat::PostSystem(manaShort ? I18N::Game::TouchNoMana : I18N::Game::TouchNoAg,
                             mu::ui::window::TYPE_ERROR_MESSAGE);
    }
    return Progress::Done;
}

// A pet command on a skill button: the Dark Lord's raven takes it at once, as a right click in the
// world sends the current one; the attack command at what the attack button would fight. Chaos Castle
// refuses them (ClassAttack.cpp). A held button sends it again once a second.
Progress CommandPet(int index)
{
    if (Hero->m_pPet == nullptr || gMapManager.InChaosCastle())
        return Progress::Done;
    const auto now = Clock::now();
    if (now < g_petCommandAt)
        return Progress::Done;
    const int target = index == AT_PET_COMMAND_TARGET ? ChooseTarget() : NoTarget;
    if (index == AT_PET_COMMAND_TARGET && target == NoTarget)
        return Progress::Done;
    if (giPetManager::SendPetCommandTo(Hero, index, target))
    {
        g_petCommandAt = now + PetCommandRepeat;
        PlayBuffer(SOUND_CLICK01);
    }
    return Progress::Done;
}

bool IsSkipped(int drop)
{
    const auto it = g_skippedDrops.find(drop);
    if (it == g_skippedDrops.end())
        return false;
    // Kept past its time, to count the asks for the same drop; another drop in the slot starts anew.
    const ITEM_t& item = Items[drop];
    const SkippedDrop& skipped = it->second;
    return Clock::now() < skipped.until && skipped.type == item.Item.Type &&
           skipped.tileX == TileOf(item.Object.Position[0]) && skipped.tileY == TileOf(item.Object.Position[1]);
}

void Skip(int drop, Clock::duration time, bool asked)
{
    const ITEM_t& item = Items[drop];
    const int type = item.Item.Type;
    const int tileX = TileOf(item.Object.Position[0]);
    const int tileY = TileOf(item.Object.Position[1]);
    SkippedDrop& skipped = g_skippedDrops[drop];
    // Another drop in the slot (the server hands a freed key on), or the last ask long ago: the count
    // starts anew, so only a drop refused twice in a row is passed over for long.
    const auto now = Clock::now();
    if (skipped.type != type || skipped.tileX != tileX || skipped.tileY != tileY ||
        now > skipped.until + RefusedDropTime)
        skipped = SkippedDrop{{}, type, tileX, tileY, 0};
    if (asked)
        ++skipped.asks;
    skipped.until = now + (skipped.asks >= RefusedDropAsks ? Clock::duration(RefusedDropTime) : time);
}

bool FitsTheBag(ITEM_t& drop)
{
    return drop.Item.Type == ITEM_ZEN || g_pMyInventory->FindEmptySlotIncludingExtensions(&drop.Item) != -1;
}

void SayBagFull()
{
    const auto now = Clock::now();
    if (now - g_fullBagSaidAt < FullBagRepeat)
        return;
    g_fullBagSaidAt = now;
    UI::Chat::PostSystem(I18N::Game::InventoryIsFull, mu::ui::window::TYPE_ERROR_MESSAGE);
}

// Taken or gone: a new drop under the same key is not the refused one.
void ForgetGoneDrops()
{
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        if (!Items[i].Object.Live)
            g_skippedDrops.erase(i);
    }
}

// The nearest drop within reach that fits the bag and was not passed over, NoDrop for none; `unfit`
// says whether a nearer one did not fit.
int NearestFittingDrop(bool* unfit)
{
    int nearest = NoDrop;
    int nearestTiles = PickupRange + 1;
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        ITEM_t& drop = Items[i];
        if (!drop.Object.Live || IsSkipped(i))
            continue;
        const int dx = Hero->PositionX - TileOf(drop.Object.Position[0]);
        const int dy = Hero->PositionY - TileOf(drop.Object.Position[1]);
        const int tiles = static_cast<int>(std::ceil(std::hypot(dx, dy)));
        if (tiles >= nearestTiles)
            continue;
        if (!FitsTheBag(drop))
        {
            if (unfit != nullptr)
                *unfit = true;
            continue;
        }
        nearestTiles = tiles;
        nearest = i;
    }
    return nearest;
}
} // namespace

// Whether the MU Helper was told the player steers the character (CMuHelper::SetSteered): the stick
// walks it, or an action button was held or owed less than a second ago. The helper's own attacks
// at another monster would fight the button's.
bool g_helperSteered = false;
bool g_stickSteersHelper = false;
Clock::time_point g_buttonsHoldHelperUntil;
constexpr auto ButtonsHoldHelperTime = std::chrono::seconds(1);

void ApplyHelperSteer()
{
    const bool steered = g_stickSteersHelper || Clock::now() < g_buttonsHoldHelperUntil;
    if (g_helperSteered == steered)
        return;
    g_helperSteered = steered;
    MUHelper::g_MuHelper.SetSteered(steered);
}

void SteerHelper(bool steered)
{
    g_stickSteersHelper = steered;
    ApplyHelperSteer();
}

void Steer(float screenX, float screenY)
{
    if (!InWorld())
        return;
    if (!g_hold.MayWalk(HerePlace()))
    {
        // Past a gate, or the controls went away: the push waits to be let go.
        EndWalk();
        SteerHelper(false);
        return;
    }
    SteerHelper(true);
    if (ButtonHoldsHero())
        return;
    float dirX = 0.f;
    float dirY = 0.f;
    if (!ScreenToMap(screenX, screenY, dirX, dirY))
        return;

    const auto now = Clock::now();
    const bool turned = !g_steering.active || dirX * g_steering.dirX + dirY * g_steering.dirY < std::cos(TurnToReplan);
    if (now - g_steering.lastWalk < (turned ? TurnRepeat : WalkRepeat))
        return;
    if (!turned && StepsLeft() > StepsLeftToReplan)
        return;

    g_steering.lastWalk = now;
    const HeroPlace from = HerePlace();
    if (WalkToward(dirX, dirY))
    {
        BreakOffFight();
        g_steering.active = true;
        g_steering.dirX = dirX;
        g_steering.dirY = dirY;
        g_steering.from = from;
    }
}

void StopSteering()
{
    g_hold.Release();
    SteerHelper(false);
    if (InWorld())
        EndWalk();
    else
        g_steering.active = false;
}

void PauseSteering()
{
    g_hold.Pause();
    SteerHelper(false);
    if (InWorld())
        EndWalk();
    else
        g_steering.active = false;
}

bool IsSteeringOnHold()
{
    return g_hold.IsWaiting();
}

bool IsSteering()
{
    return g_hold.IsHeld() && !g_hold.IsWaiting();
}

void SetPointerAims(bool aims)
{
    g_pointerAims = aims;
}

void SetPadAim(float screenX, float screenY)
{
    g_padAimX = screenX;
    g_padAimY = screenY;
}

void SetPadPlaysWorld(bool plays)
{
    g_padPlaysWorld = plays;
}

Progress Attack()
{
    // A safe zone refuses attacks: no walk out of town towards a monster either.
    if (!InWorld() || Hero->SafeZone)
        return Progress::Done;
    // A press during a swing goes off as the swing ends.
    if (Automation::IsSwingInProgress() || !ActionDue())
        return Progress::Pending;
    const int target = ChooseTarget();
    if (target == NoTarget)
        return NothingToDo();
    if (IsSteering() && !InReach(FindByKey(target), Automation::EquippedAttackRange()))
        return GiveTurnBack();

    // The target is a monster or an enemy player (IsFoe).
    const Automation::AttackResult result = Automation::AttackObject(target, true, HuntingDistance);
    if (Automation::ShouldForgetTarget(result))
        Forget(target);
    switch (result)
    {
    case Automation::AttackResult::Attacked:
        ActionWentOff();
        return Progress::Done;
    case Automation::AttackResult::NoTarget:
    case Automation::AttackResult::NoArrows:
    case Automation::AttackResult::SafeZone:
        return Progress::Done;
    default:
        return Progress::Pending;
    }
}

void SelectHotKey(int hotKey)
{
    if (InWorld() && HotKeySkillIndex(hotKey) >= 0)
        g_pSkillList->SelectHotKey(hotKey);
}

// Starts the Nova's charge, as the PC's right mouse button does (ZzzInterface.cpp, Attack).
static Progress BeginCharge(int hotKey)
{
    if (g_chargeHotKey != NoHotKey)
        return Progress::Done;
    int energy = 0;
    gSkillManager.GetSkillInformation_Energy(AT_SKILL_NOVA, &energy);
    if (CharacterAttribute->Energy + CharacterAttribute->AddEnergy < energy)
        return Progress::Done;
    const Progress progress = CastWhereHeroStands(AT_SKILL_NOVA_BEGIN, false);
    // The charge pose says it began: the cast's own result is not set for the Nova's start
    // (ClassAttack.cpp, AttackWizard returns before SkillSuccess).
    if (Hero->Object.CurrentAction != PLAYER_SKILL_HELL_BEGIN)
        return progress;
    Hero->Object.m_bySkillCount = 0;
    g_chargeHotKey = hotKey;
    g_chargeStart = Clock::now();
    return Progress::Done;
}

int ChargingHotKey()
{
    if (g_chargeHotKey != NoHotKey && Clock::now() - g_chargeStart >= LongestCharge)
        ReleaseCharge();
    return g_chargeHotKey;
}

void ReleaseCharge()
{
    if (g_chargeHotKey == NoHotKey)
        return;
    g_chargeHotKey = NoHotKey;
    if (!InWorld())
        return;
    // What the PC's release casts (ClassAttack.cpp, AT_SKILL_NOVA), sent whatever the character does
    // now: a charge the server keeps running would go off on its own later.
    SendRequestMagic(AT_SKILL_NOVA, HeroKey);
    SetAttackSpeed();
    SetAction(&Hero->Object, PLAYER_SKILL_HELL_START);
    Hero->Movement = 0;
    ActionWentOff();
}

Progress CastHotKey(int hotKey)
{
    if (!InWorld())
        return Progress::Done;
    const int index = HotKeySkillIndex(hotKey);
    if (index < 0)
        return Progress::Done;
    if (index >= AT_PET_COMMAND_DEFAULT)
        return CommandPet(index);
    const auto skill = static_cast<ActionSkillType>(CharacterAttribute->Skill[index]);
    const SkillAim aim = AimOf(skill);
    // A finger aims Teleport with a long press (the button only selects it); the pad aims it itself.
    const bool padAims = aim == SkillAim::Pointer && g_padPlaysWorld;
    // A safe zone refuses every skill (GameLogic::Combat::CanExecuteSkill).
    if ((aim == SkillAim::Pointer && !padAims) || Hero->SafeZone)
        return Progress::Done;
    // Before the turn: a skill held without mana would take every turn from a held attack button.
    if (!HasManaFor(skill))
        return ShortOfMana(skill);
    if (Automation::IsSwingInProgress() || !ActionDue())
        return Progress::Pending;

    if (padAims)
        return skill == AT_SKILL_TELEPORT_ALLY ? PadTeleportAlly(skill) : PadTeleport(skill);
    if (skill == AT_SKILL_NOVA)
        return BeginCharge(hotKey);

    const CHARACTER* friendCharacter = GoesOnFriend(skill) ? ChosenFriend() : nullptr;
    if (friendCharacter != nullptr)
        return CastAtFriend(skill, friendCharacter);
    if (GoesOnFriend(skill) && FriendOutOfSight())
        return FriendTooFar(g_friendName);
    switch (aim)
    {
    case SkillAim::Self:
        return CastWhereHeroStands(skill, true);
    case SkillAim::Around:
        return CastWhereHeroStands(skill, false);
    default:
        return CastAtMonster(skill);
    }
}

bool BindHotKey(int hotKey)
{
    if (!InWorld() || hotKey < 0 || hotKey >= HotKeyCount)
        return false;
    const int skill = Hero->CurrentSkill;
    const bool petCommand = skill >= AT_PET_COMMAND_DEFAULT && skill < AT_PET_COMMAND_END;
    if (!petCommand && (skill >= MAX_SKILLS || CharacterAttribute->Skill[skill] == 0))
        return false;
    if (g_pSkillList->GetHotKey(hotKey) == skill)
        return false;
    g_pSkillList->SetHotKey(hotKey, skill);
    SaveOptions();
    return true;
}

bool HotKeyHoldsSkill(int hotKey)
{
    return HotKeySkillIndex(hotKey) >= 0;
}

bool HotKeyShortOfMana(int hotKey)
{
    if (!InWorld())
        return false;
    const int index = HotKeySkillIndex(hotKey);
    if (index < 0 || index >= AT_PET_COMMAND_DEFAULT)
        return false;
    return !HasManaFor(static_cast<ActionSkillType>(CharacterAttribute->Skill[index]));
}

Progress PickUpNearest()
{
    if (!InWorld())
        return Progress::Done;
    if (!ActionDue())
        return Progress::Pending;

    ForgetGoneDrops();
    bool unfit = false;
    const int nearest = NearestFittingDrop(&unfit);
    if (nearest == NoDrop)
    {
        if (unfit)
            SayBagFull();
        g_dropWalkedTo = NoDrop;
        return NothingToDo();
    }

    const OBJECT& dropObject = Items[nearest].Object;
    const bool inReach =
        HeroWithin(TileOf(dropObject.Position[0]), TileOf(dropObject.Position[1]), Automation::PickupReach);
    // While the joystick walks, the button takes what is in reach.
    if (!inReach && IsSteering())
        return GiveTurnBack();
    // On its way already: a new walk each turn only sends the same path again.
    if (!inReach && Hero->Movement && g_dropWalkedTo == nearest)
        return Progress::Pending;

    switch (Automation::PickUpItem(nearest, PickupRange))
    {
    case Automation::PickupResult::Requested:
        // Passed over while the server answers: a tap gathers what lies around, on to the next drop
        // until none is left.
        Skip(nearest, AskedDropTime, true);
        g_dropWalkedTo = NoDrop;
        return Progress::Pending;
    case Automation::PickupResult::Approaching:
        g_dropWalkedTo = nearest;
        return Progress::Pending;
    case Automation::PickupResult::NoPath:
        Skip(nearest, UnreachableDropTime, false);
        return Progress::Pending;
    case Automation::PickupResult::Busy:
        // The answer for the drop before is still on its way.
        return Progress::Pending;
    case Automation::PickupResult::TooFar:
    case Automation::PickupResult::Gone:
        break;
    }
    return Progress::Done;
}

void SetForceAttack(bool on)
{
    g_forceAttack = on;
}

bool IsForceAttack()
{
    return g_forceAttack;
}

void ToggleFriend(int characterKey)
{
    const bool own = Hero != nullptr && characterKey == Hero->Key;
    g_friendKey = own || characterKey == g_friendKey ? NoTarget : characterKey;
    const CHARACTER* chosen = FindByKey(g_friendKey);
    g_friendName = chosen != nullptr ? chosen->ID : L"";
    if (g_friendName.empty())
        g_friendKey = NoTarget;
}

int FriendKey()
{
    return FriendInSight() != nullptr ? g_friendKey : NoTarget;
}

void ChooseNextFriend()
{
    if (!InWorld())
        return;
    // The party window clears the members' indexes each frame before the pad runs; this fills them again
    // (only the cleared ones, so the scene's own pass later finds them filled).
    g_pPartyManager->SearchPartyMember();
    std::vector<const CHARACTER*> members;
    for (int member = 0; member < PartyNumber; ++member)
    {
        const int index = Party[member].index;
        if (index >= 0 && index < MAX_CHARACTERS_CLIENT && IsFriend(&CharactersClient[index]))
            members.push_back(&CharactersClient[index]);
    }
    const auto chosen = std::find_if(members.begin(), members.end(),
                                     [](const CHARACTER* member) { return member->Key == g_friendKey; });
    const auto next = chosen == members.end() ? members.begin() : chosen + 1;
    if (next == members.end())
    {
        g_friendKey = NoTarget;
        UI::Chat::PostSystem(members.empty() ? I18N::Game::PadNoFriendNear : I18N::Game::PadFriendSelf,
                             mu::ui::window::TYPE_SYSTEM_MESSAGE);
        return;
    }
    g_friendKey = (*next)->Key;
    g_friendName = (*next)->ID;
    std::wstring text = I18N::Game::PadFriendChosen;
    text += (*next)->ID;
    UI::Chat::PostSystem(text, mu::ui::window::TYPE_SYSTEM_MESSAGE);
}

void TrackAim()
{
    if (!InWorld())
        return;
    const CHARACTER* aimed = AimedCharacter();
    if (aimed != nullptr && aimed->Object.Kind == KIND_PLAYER && IsGoodTarget(aimed, true))
        g_targetKey = aimed->Key;
}

void Reset()
{
    g_targetKey = NoTarget;
    g_chosenKey = NoTarget;
    g_meetKey = NoTarget;
    g_forceAttack = false;
    g_friendKey = NoTarget;
    g_friendName.clear();
    g_forgottenUntil.clear();
    g_skippedDrops.clear();
    g_chargeHotKey = NoHotKey;
    g_dropWalkedTo = NoDrop;
    g_standUntil = {};
    g_standAgainAt = {};
    g_steering.active = false;
    g_hold.Pause();
    g_buttonsHoldHelperUntil = {};
    SteerHelper(false);
}

bool CycleTarget(int step)
{
    if (!InWorld())
        return false;
    // By distance, then by key: two at the same distance keep their order between presses.
    std::vector<std::pair<int, int>> foes;
    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        const CHARACTER* character = &CharactersClient[i];
        // With the PK switch on, players too (as a tap on one would aim at them).
        if (IsGoodTarget(character, g_forceAttack))
            foes.emplace_back(TilesTo(character), character->Key);
    }
    if (foes.empty())
    {
        // No foe near (a town): the NPCs and the other players around, to talk to or to trade with.
        std::vector<std::pair<int, int>> meets;
        for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
        {
            const CHARACTER* character = &CharactersClient[i];
            if (IsGoodMeet(character))
                meets.emplace_back(TilesTo(character), character->Key);
        }
        if (meets.empty())
            return false;
        std::sort(meets.begin(), meets.end());
        g_meetKey = NextInRound(meets, MeetIndex() >= 0 ? g_meetKey : NoTarget, step);
        g_chosenKey = NoTarget;
        return true;
    }
    g_meetKey = NoTarget;
    std::sort(foes.begin(), foes.end());
    int current = g_chosenKey;
    if (current == NoTarget && TargetIndex() >= 0)
        current = CharactersClient[TargetIndex()].Key;
    g_chosenKey = g_targetKey = NextInRound(foes, current, step);
    return true;
}

void ClearChosenTarget()
{
    g_chosenKey = NoTarget;
    g_targetKey = NoTarget;
    g_meetKey = NoTarget;
}

int MeetIndex()
{
    if (!InWorld() || g_meetKey == NoTarget || g_chosenKey != NoTarget)
        return -1;
    // A foe come near takes the place of the one chosen to meet: A attacks again.
    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        if (IsGoodTarget(&CharactersClient[i], g_forceAttack))
        {
            g_meetKey = NoTarget;
            return -1;
        }
    }
    const CHARACTER* meet = FindByKey(g_meetKey);
    if (!IsGoodMeet(meet))
        return -1;
    return static_cast<int>(meet - CharactersClient);
}

bool WalkUpTo(int index, float distance)
{
    if (!InWorld() || index < 0 || index >= MAX_CHARACTERS_CLIENT)
        return false;
    const CHARACTER* character = &CharactersClient[index];
    const float dx = character->Object.Position[0] - Hero->Object.Position[0];
    const float dy = character->Object.Position[1] - Hero->Object.Position[1];
    if (std::hypot(dx, dy) < distance)
        return true;
    const auto now = Clock::now();
    if (now - g_walkUpAt >= WalkRepeat)
    {
        g_walkUpAt = now;
        Automation::WalkTo(TileOf(character->Object.Position[0]), TileOf(character->Object.Position[1]));
    }
    return false;
}

bool HasDropToPick()
{
    return NearestDropToPick() >= 0;
}

int NearestDropToPick()
{
    if (!InWorld())
        return -1;
    const int nearest = NearestFittingDrop(nullptr);
    return nearest == NoDrop ? -1 : nearest;
}

bool DropOnScreen(int index, float& x, float& y)
{
    if (index < 0 || index >= MAX_ITEMS || !Items[index].Object.Live)
        return false;
    return CameraProjection::WorldToWindowPixel(g_Camera, Items[index].Object.Position, &x, &y);
}

int NearestNpc(int tiles)
{
    if (!InWorld())
        return -1;
    int nearest = -1;
    int nearestTiles = tiles + 1;
    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        const CHARACTER* character = &CharactersClient[i];
        if (!character->Object.Live || character->Object.Kind != KIND_NPC || character->Dead != 0)
            continue;
        const int distance = TilesTo(character);
        if (distance < nearestTiles)
        {
            nearestTiles = distance;
            nearest = i;
        }
    }
    return nearest;
}

bool NpcOnScreen(int index, float& x, float& y)
{
    if (index < 0 || index >= MAX_CHARACTERS_CLIENT)
        return false;
    vec3_t position;
    VectorCopy(CharactersClient[index].Object.Position, position);
    // Its body, not its feet: the click's ray picks the model's box.
    position[2] += 80.f;
    return CameraProjection::WorldToWindowPixel(g_Camera, position, &x, &y);
}

bool FeetOnScreen(int index, float& x, float& y)
{
    if (index < 0 || index >= MAX_CHARACTERS_CLIENT)
        return false;
    return CameraProjection::WorldToWindowPixel(g_Camera, CharactersClient[index].Object.Position, &x, &y);
}

int TargetIndex()
{
    if (!InWorld() || g_targetKey == NoTarget)
        return -1;
    const CHARACTER* target = FindByKey(g_targetKey);
    if (target == nullptr || !IsGoodTarget(target, true))
        return -1;
    return static_cast<int>(target - CharactersClient);
}

void HoldHelperForButtons(bool buttonsAct)
{
    if (buttonsAct)
        g_buttonsHoldHelperUntil = Clock::now() + ButtonsHoldHelperTime;
    ApplyHelperSteer();
}
} // namespace GameLogic::Touch
