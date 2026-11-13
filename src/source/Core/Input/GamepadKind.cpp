#include "Core/Input/GamepadKind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace Core::Input::Gamepad
{
namespace
{
// Lower-case pieces of a name, any one enough. The vendor ids come as hex in the browsers' ids.
constexpr std::array<std::string_view, 6> PlayStationNames{"054c", "dualsense", "dualshock", "playstation",
                                                           "ps3", "ps4"};
constexpr std::array<std::string_view, 6> NintendoNames{"057e", "nintendo", "switch", "pro controller",
                                                        "joy-con", "joycon"};

bool HasAny(const std::string& name, const std::array<std::string_view, 6>& pieces)
{
    return std::any_of(pieces.begin(), pieces.end(),
                       [&name](std::string_view piece) { return name.find(piece) != std::string::npos; });
}
} // namespace

PadKind KindFromName(std::string_view name)
{
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // "ps5" also ends many a Sony name; a bare "Wireless Controller" is how a DualShock 4 names itself.
    if (HasAny(lower, PlayStationNames) || lower.find("ps5") != std::string::npos ||
        lower == "wireless controller")
        return PadKind::PlayStation;
    if (HasAny(lower, NintendoNames))
        return PadKind::Nintendo;
    return PadKind::Xbox;
}
} // namespace Core::Input::Gamepad
