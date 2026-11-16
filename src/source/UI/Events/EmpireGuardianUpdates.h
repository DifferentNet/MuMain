#pragma once

#include <cstdint>

namespace UI::EmpireGuardian
{
void SetEntryInfo(int day, int zone, int remainingTick);
void UpdateTimer(int type, int remainingTick, int monsterCount);
void ShowZoneCleared();
void ShowFinalReward(int experience);
// Kundun's Lair's win or loss, shown when the result comes on a Kalima map.
void ShowSoloKalimaResult(bool cleared);
void HideTimer();
}
