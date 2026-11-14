// Which maker's buttons a pad wears, for the glyphs the game draws on its controls. The game reads
// every pad by place (SDL's south, east, west and north buttons), so a DualSense's cross and a
// Switch pad's B both confirm, as an Xbox pad's A does; only the drawn names differ.
#pragma once

#include <cstdint>
#include <string_view>

namespace Core::Input::Gamepad
{
enum class PadKind : std::uint8_t
{
    Xbox,
    PlayStation,
    Nintendo,
};

// The player's choice of glyphs (Options -> Interface -> Button glyphs): Auto follows the pad.
enum class GlyphChoice : std::uint8_t
{
    Auto,
    Xbox,
    PlayStation,
    Nintendo,
};
inline constexpr int GlyphChoiceCount = 4;

// A pad's kind guessed from its name: SDL's name on a PC, the browser's Gamepad.id on /play (as
// "DualSense Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 0ce6)" in Chrome or
// "054c-0ce6-DualSense Wireless Controller" in Firefox). Sony's and Nintendo's USB vendor ids count
// too. Anything else reads as Xbox, the layout SDL and the browsers map every pad to.
[[nodiscard]] PadKind KindFromName(std::string_view name);

// The kind whose glyphs show: the player's choice, or with Auto the pad's own.
[[nodiscard]] constexpr PadKind ShownKind(GlyphChoice choice, PadKind pad)
{
    switch (choice)
    {
    case GlyphChoice::Xbox:
        return PadKind::Xbox;
    case GlyphChoice::PlayStation:
        return PadKind::PlayStation;
    case GlyphChoice::Nintendo:
        return PadKind::Nintendo;
    case GlyphChoice::Auto:
        break;
    }
    return pad;
}
} // namespace Core::Input::Gamepad
