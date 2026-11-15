#pragma once

#include "Engine/Monsters/MonsterPreloadQueue.h"

enum EMonsterType : int;

namespace Engine::Monsters
{
    // Monster preload (config.ini [Render] PreloadMonsters, on by default). On a map change the game loads
    // the model, textures and sounds of every monster type the server spawns on that map (the list is
    // Data/Local/MapMonsters.txt), so a type's first appearance doesn't stall the game: about 1.5 s of it
    // behind the loading screen, the rest one type a frame while the map's loading countdown runs, at most
    // 40 types and 256 MiB of textures a map. What doesn't fit loads when it first shows, as before.

    // Loads one type the way its first spawn would: creates it on the hero's tile under a key the server
    // never sends and deletes it at once, with the effects, joints and objects its creation started.
    WarmUpResult WarmUp(EMonsterType type);

    // The hero has entered `map` (its world is loaded, the hero placed): queues the map's types and warms
    // them for the loading screen's share of the time.
    void PreloadMap(int map);

    // One queued type of the current map, called each frame while the loading countdown runs.
    void RunPendingStep();

    // True while WarmUp creates a monster: model loads then are the preload's, not a late load.
    bool IsWarmingUp();
}
