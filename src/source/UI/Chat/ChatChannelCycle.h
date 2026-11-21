// The chat's channels in the order the pad's left stick click steps them (pad round 8, B1): All, Party,
// Guild, Gens, the chat bar's four buttons left to right (CChatInputBox::INPUT_CHAT_MESSAGE + channel). The
// ones the hero is in none of are passed by. Pure, for the tests.
#pragma once

namespace UI::Chat::ChannelCycle
{
inline constexpr int All = 0;
inline constexpr int Party = 1;
inline constexpr int Guild = 2;
inline constexpr int Gens = 3;
inline constexpr int Count = 4;

// Whether the hero can speak there: All always, the others while he is in a party, a guild, a gens.
[[nodiscard]] bool IsOpen(int channel, bool inParty, bool inGuild, bool inGens);

// The channel after `current` the hero is in, round to All. From a channel he is not in (he left the party
// meanwhile) or one out of range, All.
[[nodiscard]] int NextChannel(int current, bool inParty, bool inGuild, bool inGens);
} // namespace UI::Chat::ChannelCycle
