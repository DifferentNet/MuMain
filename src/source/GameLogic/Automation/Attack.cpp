#include "stdafx.h"
#include "GameLogic/Automation/Attack.h"

#include "Character/CharacterManager.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/PlayerActionState.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Items/ItemCategories.h"

#include <algorithm>
#include <chrono>

// Movement/target globals are defined in ZzzInterface.cpp.
extern int SelectedCharacter;
extern int TargetX;
extern int TargetY;

namespace GameLogic::Automation
{
namespace
{
// How long an automated send waits for its swing to show before the next one may go. A normal attack or cast
// starts its swing in the same frame, so this only matters when something keeps the animation from playing:
// the caller then sends at most 4 a second instead of one per frame.
constexpr auto SwingStartWait = std::chrono::milliseconds(250);
std::chrono::steady_clock::time_point s_lastSend{};
bool s_awaitingSwing = false;
} // namespace

void NoteAutomatedSend()
{
    s_lastSend = std::chrono::steady_clock::now();
    s_awaitingSwing = true;
}

void ForgetAutomatedSend()
{
    s_awaitingSwing = false;
}

bool IsSwingInProgress()
{
    // The stance animations inside the attack enum range don't count: otherwise a
    // Fenrir-mounted idle character (PLAYER_FENRIR_STAND) reads as a perpetual swing
    // and no automated action ever fires. Fenrir attack/skill actions sit below
    // PLAYER_FENRIR_RUN, so they stay gated and cadence still tracks AttackSpeed.
    //
    // Stunned or asleep counts as busy: the animation stops then (CharacterAnimation returns false on every
    // frame and the hero is put back to standing), so no swing ever shows and an automated caller sent a new
    // attack on every frame, 60-130 a second, all refused by the server ("attacking in stunned state";
    // Sephiroth's MU Helper in a Tier Map, 2026-11-03). A player's own attacks wait the same way (MoveHero).
    if (g_isCharacterBuff((&Hero->Object), eDeBuff_Stun) || g_isCharacterBuff((&Hero->Object), eDeBuff_Sleep))
    {
        return true;
    }

    if (Engine::Object::IsSwingAction(Hero->Object.CurrentAction))
    {
        s_awaitingSwing = false;
        return true;
    }

    if (s_awaitingSwing)
    {
        if (std::chrono::steady_clock::now() - s_lastSend < SwingStartWait)
        {
            return true;
        }
        s_awaitingSwing = false;
    }

    return false;
}

// Steps of the planned path that are walked per call. Re-planning every
// frame keeps a moving target reachable without committing to a long walk.
constexpr int StepsPerApproach = 2;

// Walks the first steps of `path` towards the target and sends the move.
static void ApproachAlong(const PATH_t& path)
{
    Hero->Path.Lock.lock();
    const int steps = std::min<int>(path.PathNum, StepsPerApproach);
    for (int step = 0; step < steps; ++step)
    {
        Hero->Path.PathX[step] = path.PathX[step];
        Hero->Path.PathY[step] = path.PathY[step];
    }
    Hero->Path.PathNum = static_cast<unsigned char>(steps);
    Hero->Path.CurrentPath = 0;
    Hero->Path.CurrentPathFloat = 0;
    Hero->Path.Lock.unlock();

    // A walk the engine can recognise: without this the character keeps the
    // movement mode of whatever it was doing before (MOVEMENT_ATTACK after a
    // swing), which is not what a walk towards a target is.
    Hero->MovementType = MOVEMENT_MOVE;
    SendMove(Hero, &Hero->Object);
}

// The reach of whatever is in the character's hands right now.
float EquippedAttackRange()
{
    const int rightHand = CharacterMachine->Equipment[EQUIPMENT_WEAPON_RIGHT].Type;
    const bool spear = GameLogic::Items::IsSpearType(rightHand);
    const bool bow = gCharacterManager.GetEquipedBowType() != BOWTYPE_NONE;
    return BasicAttackRange(spear, bow);
}

AttackResult AttackObject(int targetKey, bool allowPlayers, int huntingDistance)
{
    if (targetKey == -1)
    {
        return AttackResult::NoTarget;
    }

    // Let the current swing finish before attacking again, so the cadence
    // tracks AttackSpeed instead of the caller's timer.
    if (IsSwingInProgress())
    {
        return AttackResult::Busy;
    }

    const int targetIndex = FindCharacterIndex(targetKey);
    if (targetIndex == MAX_CHARACTERS_CLIENT)
    {
        return AttackResult::NotInView;
    }

    CHARACTER* target = &CharactersClient[targetIndex];
    if (target->Dead > 0 || (!allowPlayers && !IsMonster(target)))
    {
        return AttackResult::NotAttackable;
    }

    const float range = EquippedAttackRange();

    // The target tile is what the path finder and the wall test read.
    TargetX = (int)(target->Object.Position[0] / TERRAIN_SCALE);
    TargetY = (int)(target->Object.Position[1] / TERRAIN_SCALE);

    // CheckWall reads the global SelectedCharacter on every step of its walk
    // (ZzzInterface.cpp) to exempt gate models from the terrain-wall test —
    // gates stand on no-move tiles, so without the exemption they can never
    // be attacked. The selection therefore has to be this target *before*
    // the checks, as the helper path this was extracted from did. A refused
    // attack still must not leave the UI pointing at something it will not
    // hit, so the previous selection is put back on the refusal paths.
    const int previousSelection = SelectedCharacter;
    SelectedCharacter = targetIndex;

    // Range first, then a path only when one is needed — the order the
    // engine's own attack loop uses (ZzzInterface.cpp:1282, 1349). The other
    // way round refuses a target that is standing in range but that no walk
    // reaches: across a fence or a stretch of water, where the point of
    // having range is that the walk is not needed.
    const bool inRange = CheckTile(Hero, &Hero->Object, range);
    if (inRange && !CheckWall(Hero->PositionX, Hero->PositionY, TargetX, TargetY))
    {
        SelectedCharacter = previousSelection;
        return AttackResult::Blocked;
    }

    PATH_t path;
    if (!inRange && !PathFinding2(Hero->PositionX, Hero->PositionY, TargetX, TargetY, &path,
                                  static_cast<float>(huntingDistance) + range))
    {
        SelectedCharacter = previousSelection;
        return AttackResult::NoPath;
    }

    if (!inRange)
    {
        ApproachAlong(path);
        return AttackResult::Approaching;
    }

    if (gCharacterManager.GetEquipedBowType() != BOWTYPE_NONE && !CheckArrow())
    {
        return AttackResult::NoArrows;
    }

    // Action() drops the hit packet silently inside a safe zone; say so
    // rather than reporting a swing that never reached the server.
    if (Hero->SafeZone)
    {
        return AttackResult::SafeZone;
    }

    Hero->MovementType = MOVEMENT_ATTACK;
    ActionTarget = targetIndex;
    Attacking = 1;
    Action(Hero, &Hero->Object, true);
    NoteAutomatedSend();
    return AttackResult::Attacked;
}
} // namespace GameLogic::Automation
