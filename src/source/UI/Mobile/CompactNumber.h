// MU Moldova: a gauge number short enough for the phone's small SD and AG gauges. Their boxes hold
// about six digits at the phone's text size, so "32767 / 32767" was cut off; a big number is now
// shown in whole thousands or millions ("32k/32k").
#pragma once

#include <cstdint>
#include <string>

namespace UI::Mobile::CompactNumber
{
// Numbers below this are shown as they are.
inline constexpr std::uint32_t ExactBelow = 10'000;

// The value as it is below ExactBelow, in whole thousands with 'k' below a million, else in whole
// millions with 'M'. It rounds down, so a full gauge never reads more than it holds.
[[nodiscard]] std::string Format(std::uint32_t value);
} // namespace UI::Mobile::CompactNumber
