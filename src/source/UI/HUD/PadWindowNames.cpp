#include "UI/HUD/PadWindowNames.h"

#include <algorithm>
#include <array>

namespace UI::HUD::PadWindowNames
{
namespace
{
constexpr std::array<std::string_view, 5> NotWindows = {"main_frame.rml", "pad_overlay.rml", "tooltip.rml",
                                                        "cursor_item.rml", "item_explanation.rml"};
constexpr std::string_view SkillPicker = "mu_helper_skill_picker.rml";
constexpr std::array<std::string_view, 5> FloatingWindows = {SkillPicker, "friend_shell.rml", "letter_read.rml",
                                                             "letter_write.rml", "chat_room.rml"};
constexpr std::string_view ReconnectDialog = "reconnect_dialog.rml";

template <std::size_t Count>
bool IsOneOf(const std::array<std::string_view, Count>& names, std::string_view name)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}
} // namespace

bool IsNotWindow(std::string_view name)
{
    return IsOneOf(NotWindows, name);
}

bool IsFloatingWindow(std::string_view name)
{
    return IsOneOf(FloatingWindows, name);
}

bool IsPopupWindow(std::string_view name)
{
    return name == SkillPicker;
}

bool IsWorldDialog(std::string_view name)
{
    return name == ReconnectDialog;
}
} // namespace UI::HUD::PadWindowNames
