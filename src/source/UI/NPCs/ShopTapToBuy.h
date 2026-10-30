// MU Moldova: a finger buys from an NPC's shop with two taps on the item (CNPCShop). The mouse buys
// with one click, as it always has; on a phone a finger brushing over the shop must not spend the
// player's zen. The first tap chooses the item (it lights up and its tooltip shows), a second tap on
// it soon after buys it, and a tap on another item chooses that one instead.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace UI::Shop::TapToBuy
{
using Clock = std::chrono::steady_clock;

// How long after the first tap a second one on the same item still buys it.
inline constexpr auto SecondTapTime = std::chrono::milliseconds(1500);

// The item a first tap chose (its ITEM::Key), and when.
struct Choice
{
    std::optional<std::uint32_t> itemKey;
    Clock::time_point time;
};

enum class Action : std::uint8_t
{
    // The item is chosen now: it waits for the second tap.
    Choose,
    // The second tap on the chosen item: buy it.
    Buy,
};

// The item a second tap at `now` would buy, if any.
[[nodiscard]] std::optional<std::uint32_t> Chosen(const Choice& choice, Clock::time_point now);

// A tap on the shop's item `itemKey` at `now`. `choice` is kept up to date: it holds the item after
// Choose and nothing after Buy, so a third tap chooses again.
[[nodiscard]] Action Tap(Choice& choice, std::uint32_t itemKey, Clock::time_point now);
} // namespace UI::Shop::TapToBuy
