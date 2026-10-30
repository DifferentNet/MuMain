#include "stdafx.h"
#include "GameLogic/Touch/TouchActions.h"

#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzOpenData.h"
#include "GameLogic/Automation/Attack.h"
#include "GameLogic/Automation/Movement.h"
#include "GameLogic/Automation/Pickup.h"
#include "GameLogic/Automation/Skill.h"
#include "UI/Core/WindowSystem.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>

// Defined in ZzzInterface.cpp: the character under the pointer, which a tap on a monster leaves there.
extern int SelectedCharacter;

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

constexpr int NoTarget = -1;

struct Steering
{
    bool active = false;
    // The direction of the walk under way, on the map (tiles).
    float dirX = 0.f;
    float dirY = 0.f;
    Clock::time_point lastWalk;
};

Steering g_steering;
int g_targetKey = NoTarget;
std::map<int, Clock::time_point> g_forgottenUntil;
Clock::time_point g_lastAction;

bool InWorld()
{
    return Hero != nullptr && Hero->Object.Live && Hero->Dead == 0;
}

bool ActionDue()
{
    const auto now = Clock::now();
    if (now - g_lastAction < ActionRepeat)
        return false;
    g_lastAction = now;
    return true;
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

int StepsLeft()
{
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

bool IsHuntable(const CHARACTER* character)
{
    return character != nullptr && character->Object.Live && character->Dead == 0 && character != Hero &&
           IsMonster(const_cast<CHARACTER*>(character));
}

// Tiles to the monster, where it stands or where it is walking to, whichever is nearer: as the MU
// Helper measures it.
int TilesTo(const CHARACTER* character)
{
    const auto distance = [](int dx, int dy) { return static_cast<int>(std::ceil(std::hypot(dx, dy))); };
    return std::min(distance(Hero->PositionX - character->PositionX, Hero->PositionY - character->PositionY),
                    distance(Hero->PositionX - character->TargetX, Hero->PositionY - character->TargetY));
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

bool IsGoodTarget(const CHARACTER* character)
{
    return IsHuntable(character) && !IsForgotten(character->Key) && TilesTo(character) <= TargetRange;
}

// The monster the player tapped (the pointer stays on it), else the one fought last, else the nearest.
int ChooseTarget()
{
    if (SelectedCharacter >= 0 && SelectedCharacter < MAX_CHARACTERS_CLIENT &&
        IsGoodTarget(&CharactersClient[SelectedCharacter]))
        return g_targetKey = CharactersClient[SelectedCharacter].Key;

    if (IsGoodTarget(FindByKey(g_targetKey)))
        return g_targetKey;

    int nearestKey = NoTarget;
    int nearestTiles = TargetRange + 1;
    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        const CHARACTER* character = &CharactersClient[i];
        if (!IsGoodTarget(character))
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
// character (the MU Helper's buff table, CMuHelper::BuffTarget, and the heals).
enum class SkillAim
{
    Monster,
    Self,
    Around,
    // Aimed where the player points (a long press on the ground): the button only selects it.
    Pointer,
};

SkillAim AimOf(ActionSkillType skill)
{
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
        return SkillAim::Around;
    case AT_SKILL_TELEPORT:
    case AT_SKILL_TELEPORT_ALLY:
        return SkillAim::Pointer;
    default:
        return SkillAim::Monster;
    }
}

// The skill-list index in a hotkey slot of the bar, or -1 for an empty slot.
int HotKeySlotSkillIndex(int slot)
{
    const int hotKey = g_pSkillList->GetHotKeySlotNumber(slot);
    return hotKey < 0 ? -1 : g_pSkillList->GetHotKey(hotKey);
}

void CastAtMonster(ActionSkillType skill)
{
    const int target = ChooseTarget();
    if (target == NoTarget)
    {
        // No monster around: an area skill still goes off where the character stands.
        Automation::CastSkill(skill, false, NoTarget, false, HuntingDistance);
        return;
    }
    const Automation::SkillResult result = Automation::CastSkill(skill, true, target, false, HuntingDistance);
    if (Automation::ShouldForgetTarget(result) && result != Automation::SkillResult::Refused)
        Forget(target);
}
} // namespace

void Steer(float screenX, float screenY)
{
    if (!InWorld())
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
    if (WalkToward(dirX, dirY))
    {
        g_steering.active = true;
        g_steering.dirX = dirX;
        g_steering.dirY = dirY;
    }
}

void StopSteering()
{
    if (!g_steering.active)
        return;
    g_steering.active = false;
    if (Hero == nullptr)
        return;
    // As releasing the mouse: the character stops at the tile it reached, the server is told nothing.
    Hero->Path.Lock.lock();
    Hero->Path.PathNum = 0;
    Hero->Path.CurrentPath = 0;
    Hero->Path.CurrentPathFloat = 0;
    Hero->Path.Lock.unlock();
}

void Attack()
{
    if (!InWorld() || !ActionDue())
        return;
    const int target = ChooseTarget();
    if (target == NoTarget)
        return;
    const Automation::AttackResult result = Automation::AttackObject(target, false, HuntingDistance);
    if (Automation::ShouldForgetTarget(result))
        Forget(target);
}

void SelectHotKeySlot(int slot)
{
    if (InWorld() && HotKeySlotSkillIndex(slot) >= 0)
        g_pSkillList->OnHotkeySlotClick(slot);
}

void CastHotKeySlot(int slot)
{
    if (!InWorld())
        return;
    const int index = HotKeySlotSkillIndex(slot);
    // Pet commands only switch the pet's behaviour, as the slot's click does.
    if (index < 0 || index >= AT_PET_COMMAND_DEFAULT || index >= MAX_SKILLS)
        return;
    const auto skill = static_cast<ActionSkillType>(CharacterAttribute->Skill[index]);
    if (skill == 0 || !ActionDue())
        return;

    switch (AimOf(skill))
    {
    case SkillAim::Self:
        Automation::CastSkill(skill, true, Hero->Key, true, HuntingDistance);
        break;
    case SkillAim::Around:
        Automation::CastSkill(skill, false, NoTarget, true, HuntingDistance);
        break;
    case SkillAim::Pointer:
        break;
    case SkillAim::Monster:
        CastAtMonster(skill);
        break;
    }
}

bool BindHotKeySlot(int slot)
{
    if (!InWorld() || slot < 0 || slot >= 5)
        return false;
    const int skill = Hero->CurrentSkill;
    const bool petCommand = skill >= AT_PET_COMMAND_DEFAULT && skill < AT_PET_COMMAND_END;
    if (!petCommand && (skill >= MAX_SKILLS || CharacterAttribute->Skill[skill] == 0))
        return false;
    // The bar shows the hotkeys 1-5, or 6-9 and 0 when switched up (CSkillList::GetHotKeySlotNumber).
    int hotKey = (g_pSkillList->IsSkillListUp() ? 6 : 1) + slot;
    if (hotKey == 10)
        hotKey = 0;
    if (g_pSkillList->GetHotKey(hotKey) == skill)
        return false;
    g_pSkillList->SetHotKey(hotKey, skill);
    SaveOptions();
    return true;
}

void PickUpNearest()
{
    if (!InWorld() || !ActionDue())
        return;
    int nearest = -1;
    int nearestTiles = PickupRange + 1;
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        const ITEM_t& drop = Items[i];
        if (!drop.Object.Live)
            continue;
        const int dx = Hero->PositionX - static_cast<int>(drop.Object.Position[0] / TERRAIN_SCALE);
        const int dy = Hero->PositionY - static_cast<int>(drop.Object.Position[1] / TERRAIN_SCALE);
        const int tiles = static_cast<int>(std::ceil(std::hypot(dx, dy)));
        if (tiles < nearestTiles)
        {
            nearestTiles = tiles;
            nearest = i;
        }
    }
    if (nearest >= 0)
        Automation::PickUpItem(nearest, PickupRange);
}

void Reset()
{
    g_targetKey = NoTarget;
    g_forgottenUntil.clear();
    g_steering = Steering{};
}
} // namespace GameLogic::Touch
