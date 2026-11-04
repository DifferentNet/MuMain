#pragma once

#include "Core/Globals/_enum.h"

namespace Engine::Object
{
    // True while a player's action is one of the attack/skill swing animations
    // (PLAYER_ATTACK_FIST .. PLAYER_RIDE_SKILL). The swing animation's playback
    // speed scales with AttackSpeed (see SetAttackSpeed in ZzzCharacter.cpp), so
    // this predicate doubles as the natural attack-cadence gate: hold off the
    // next action until the current swing finishes, and the rate follows attack
    // speed instead of any fixed timer.
    inline bool IsAttackAction(int currentAction)
    {
        return currentAction >= PLAYER_ATTACK_FIST && currentAction <= PLAYER_RIDE_SKILL;
    }

    // True while a player is in an actual attack or skill swing: IsAttackAction without the
    // stance animations that share its enum range (mounted idle/walk/run, the two-hand sword
    // stance, ride-horse, rage-fenrir), which MoveHero (ZzzInterface.cpp) excludes the same way.
    inline bool IsSwingAction(int currentAction)
    {
        if (!IsAttackAction(currentAction))
        {
            return false;
        }

        return !((currentAction >= PLAYER_STOP_TWO_HAND_SWORD_TWO && currentAction <= PLAYER_RUN_TWO_HAND_SWORD_TWO) ||
                 (currentAction >= PLAYER_DARKLORD_STAND && currentAction <= PLAYER_RUN_RIDE_HORSE) ||
                 (currentAction >= PLAYER_FENRIR_RUN && currentAction <= PLAYER_FENRIR_WALK_ONE_LEFT) ||
                 (currentAction >= PLAYER_RAGE_FENRIR_WALK && currentAction <= PLAYER_RAGE_FENRIR_STAND_ONE_LEFT));
    }

    // True when a player's action lets a skill start: one of the stand poses (on foot, mounted,
    // two-hand sword, Fenrir, the Rage Fighter's). ExecuteSkill starts no skill from any other
    // action: not while walking, nor while flinching from a hit.
    inline bool IsSkillReadyAction(int currentAction)
    {
        return !(currentAction < PLAYER_STOP_MALE
            || currentAction > PLAYER_STOP_RIDE_WEAPON
            && currentAction != PLAYER_STOP_TWO_HAND_SWORD_TWO
            && currentAction != PLAYER_SKILL_HELL_BEGIN
            && currentAction != PLAYER_DARKLORD_STAND
            && currentAction != PLAYER_STOP_RIDE_HORSE
            && currentAction != PLAYER_FENRIR_STAND
            && currentAction != PLAYER_FENRIR_STAND_TWO_SWORD
            && currentAction != PLAYER_FENRIR_STAND_ONE_RIGHT
            && currentAction != PLAYER_FENRIR_STAND_ONE_LEFT
            && !(currentAction >= PLAYER_RAGE_FENRIR_STAND && currentAction <= PLAYER_RAGE_FENRIR_STAND_ONE_LEFT)
            && currentAction != PLAYER_RAGE_UNI_STOP_ONE_RIGHT
            && currentAction != PLAYER_STOP_RAGEFIGHTER);
    }

    // True while a player is sitting or holding a pose (PLAYER_SIT1 ..
    // PLAYER_POSE_FEMALE1). Used to keep these animations from being reset to the
    // stand pose when equipment or class changes.
    inline bool IsSitOrPoseAction(int currentAction)
    {
        return currentAction >= PLAYER_SIT1 && currentAction <= PLAYER_POSE_FEMALE1;
    }
}
