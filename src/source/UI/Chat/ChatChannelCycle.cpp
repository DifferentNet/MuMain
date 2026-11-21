#include "UI/Chat/ChatChannelCycle.h"

namespace UI::Chat::ChannelCycle
{
bool IsOpen(int channel, bool inParty, bool inGuild, bool inGens)
{
    switch (channel)
    {
    case All:
        return true;
    case Party:
        return inParty;
    case Guild:
        return inGuild;
    case Gens:
        return inGens;
    default:
        return false;
    }
}

int NextChannel(int current, bool inParty, bool inGuild, bool inGens)
{
    if (!IsOpen(current, inParty, inGuild, inGens))
        return All;
    for (int step = 1; step < Count; ++step)
    {
        const int channel = (current + step) % Count;
        if (IsOpen(channel, inParty, inGuild, inGens))
            return channel;
    }
    return All;
}
} // namespace UI::Chat::ChannelCycle
