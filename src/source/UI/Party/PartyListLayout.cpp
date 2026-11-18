#include "UI/Party/PartyListLayout.h"

#include <algorithm>

float UI::Party::List::HealthBarLength(int stepHP)
{
    const int step = std::clamp(stepHP, 0, HealthSteps);
    return static_cast<float>(step) / static_cast<float>(HealthSteps) * static_cast<float>(HealthBarWidth);
}

bool UI::Party::List::IsLowHealth(bool onMap, int stepHP)
{
    return onMap && stepHP <= LowHealthSteps;
}
