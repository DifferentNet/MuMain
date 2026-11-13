#include "stdafx.h"
#include "UI/HUD/PadCard.h"

namespace
{
struct State
{
    bool open = false;
    int page = UI::HUD::PadCard::WorldPage;
};
State g_state;
} // namespace

namespace UI::HUD::PadCard
{
void Open()
{
    g_state.open = true;
    g_state.page = WorldPage;
}

void Close()
{
    g_state.open = false;
}

bool IsOpen()
{
    return g_state.open;
}

void Turn(int step)
{
    g_state.page = ((g_state.page + step) % PageCount + PageCount) % PageCount;
}

int Page()
{
    return g_state.page;
}
} // namespace UI::HUD::PadCard
