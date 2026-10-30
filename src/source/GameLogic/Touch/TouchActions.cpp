#include "stdafx.h"
#include "GameLogic/Touch/TouchActions.h"

#include "Audio/DSPlaySound.h"
#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInterface.h"
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
#include "UI/Chat/ChatMessages.h"
#include "UI/Core/WindowSystem.h"
#include "World/GameMaps/GMBattleCastle.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>

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
// How often the pickup button says the bag is full.
constexpr auto FullBagRepeat = std::chrono::seconds(3);
// How often a skill button short of mana or AG asks for a mana potion and says so.
constexpr auto ShortOfManaRepeat = std::chrono::milliseconds(1500);
// How often a held pet command button sends its command again.
constexpr auto PetCommandRepeat = std::chrono::seconds(1);

// How far ahead the joystick walks, in tiles, and the shorter walk tried along a wall.
constexpr int StickTiles = 4;
constexpr int ShortStickTiles = 2;
// The joystick turned this far (radians) from the walk under way: a new walk at once.
constexpr float TurnToReplan = 0.5f;
// A walk with this many steps left is extended before the character stops.
constexpr int StepsLeftToReplan = 2;

// Monsters within this many tiles are fought by the attack and skill buttons.
constexpr int TargetRange = 8;
// How far beyond its reach the path finder may look for a way to the target.
constexpr int HuntingDistance = 8;
// Drops within this many tiles are gathered by the pickup button.
constexpr int PickupRange = 6;
// How far from the character Hellfire's and Inferno's fire reaches, in tiles: cast where the
// character stands, they hit nothing farther away.
constexpr float FireRingTiles = 3.f;

constexpr int NoTarget = -1;
constexpr int NoDrop = -1;

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
int g_targetKey = NoTarget;
std::map<int, Clock::time_point> g_forgottenUntil;
Clock::time_point g_nextAction;
// The drops passed over (asked for, or out of a walk's reach), by their slot, until when.
std::map<int, Clock::time_point> g_skippedDrops;
// The drop the pickup button walks to, and when the bag was last said to be full.
int g_dropWalkedTo = NoDrop;
Clock::time_point g_fullBagSaidAt;
// The PK switch (SetForceAttack) and the party member the heals and buffs go to (ToggleFriend).
bool g_forceAttack = false;
int g_friendKey = NoTarget;
// When a skill short of mana or AG may ask and say so again, and when a held pet command may go out
// again.
Clock::time_point g_shortOfManaAt;
Clock::time_point g_petCommandAt;

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
    g_nextAction = now + ActionRepeat;
    return true;
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

bool IsFriend(const CHARACTER* character)
{
    return character != nullptr && character != Hero && character->Object.Live && character->Dead == 0 &&
           character->Object.Kind == KIND_PLAYER && g_pPartyManager->IsPartyMember(IndexOf(character)) &&
           TilesTo(character) <= TargetRange;
}

// The party member a heal or a buff goes to: the one the pad's pointer is on, else the one whose
// party card a tap chose, while near; none for the character itself.
const CHARACTER* ChosenFriend()
{
    if (g_pointerAims && SelectedCharacter >= 0 && SelectedCharacter < MAX_CHARACTERS_CLIENT &&
        IsFriend(&CharactersClient[SelectedCharacter]))
        return &CharactersClient[SelectedCharacter];
    const CHARACTER* chosen = FindByKey(g_friendKey);
    return IsFriend(chosen) ? chosen : nullptr;
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
        return Progress::Pending;
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
        return IsSteering() ? Progress::Done : CastWhereHeroStands(skill, false);
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
        return Progress::Pending;
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
            return Progress::Pending;
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
        return Progress::Pending;
    if (inReach)
    {
        if (!GameLogic::Combat::CanExecuteSkill(Hero, skill, distance))
            return Progress::Pending;
        if (!HeroStands())
            return Progress::Pending;
    }
    return OutcomeOf(Automation::CastSkill(skill, true, friendCharacter->Key, true, HuntingDistance));
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
    UI::Chat::PostSystem(manaShort ? I18N::Game::TouchNoMana : I18N::Game::TouchNoAg,
                         mu::ui::window::TYPE_ERROR_MESSAGE);
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
    if (Clock::now() < it->second)
        return true;
    g_skippedDrops.erase(it);
    return false;
}

void Skip(int drop, Clock::duration time)
{
    g_skippedDrops[drop] = Clock::now() + time;
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
} // namespace

// Whether the MU Helper was told the stick walks the character (CMuHelper::SetSteered).
bool g_helperSteered = false;

void SteerHelper(bool steered)
{
    if (g_helperSteered == steered)
        return;
    g_helperSteered = steered;
    MUHelper::g_MuHelper.SetSteered(steered);
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
    if (now - g_steering.lastWalk < WalkRepeat)
        return;
    const bool turned = !g_steering.active || dirX * g_steering.dirX + dirY * g_steering.dirY < std::cos(TurnToReplan);
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
        return Progress::Done;
    if (IsSteering() && !InReach(FindByKey(target), Automation::EquippedAttackRange()))
        return Progress::Pending;

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
    // A safe zone refuses every skill (GameLogic::Combat::CanExecuteSkill).
    if (aim == SkillAim::Pointer || Hero->SafeZone)
        return Progress::Done;
    // Before the turn: a skill held without mana would take every turn from a held attack button.
    if (!HasManaFor(skill))
        return ShortOfMana(skill);
    if (Automation::IsSwingInProgress() || !ActionDue())
        return Progress::Pending;

    const CHARACTER* friendCharacter = GoesOnFriend(skill) ? ChosenFriend() : nullptr;
    if (friendCharacter != nullptr)
        return CastAtFriend(skill, friendCharacter);
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

Progress PickUpNearest()
{
    if (!InWorld())
        return Progress::Done;
    if (!ActionDue())
        return Progress::Pending;

    int nearest = NoDrop;
    int nearestTiles = PickupRange + 1;
    bool unfit = false;
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
            unfit = true;
            continue;
        }
        nearestTiles = tiles;
        nearest = i;
    }
    if (nearest == NoDrop)
    {
        if (unfit)
            SayBagFull();
        g_dropWalkedTo = NoDrop;
        return Progress::Done;
    }

    const OBJECT& dropObject = Items[nearest].Object;
    const bool inReach =
        HeroWithin(TileOf(dropObject.Position[0]), TileOf(dropObject.Position[1]), Automation::PickupReach);
    // While the joystick walks, the button takes what is in reach.
    if (!inReach && IsSteering())
        return Progress::Pending;
    // On its way already: a new walk each turn only sends the same path again.
    if (!inReach && Hero->Movement && g_dropWalkedTo == nearest)
        return Progress::Pending;

    switch (Automation::PickUpItem(nearest, PickupRange))
    {
    case Automation::PickupResult::Requested:
        // Passed over while the server answers: a tap gathers what lies around, on to the next drop
        // until none is left.
        Skip(nearest, AskedDropTime);
        g_dropWalkedTo = NoDrop;
        return Progress::Pending;
    case Automation::PickupResult::Approaching:
        g_dropWalkedTo = nearest;
        return Progress::Pending;
    case Automation::PickupResult::NoPath:
        Skip(nearest, UnreachableDropTime);
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
}

int FriendKey()
{
    return IsFriend(FindByKey(g_friendKey)) ? g_friendKey : NoTarget;
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
    g_forceAttack = false;
    g_friendKey = NoTarget;
    g_forgottenUntil.clear();
    g_skippedDrops.clear();
    g_dropWalkedTo = NoDrop;
    g_standUntil = {};
    g_standAgainAt = {};
    g_steering.active = false;
    g_hold.Pause();
    SteerHelper(false);
}
} // namespace GameLogic::Touch
