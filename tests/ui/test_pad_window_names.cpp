#include <doctest.h>

#include "UI/HUD/PadWindowNames.h"

namespace Names = UI::HUD::PadWindowNames;

TEST_CASE("the HUD's bar, the pad's frame and the tooltips are no windows")
{
    for (const char* name : {"main_frame.rml", "pad_overlay.rml", "tooltip.rml", "cursor_item.rml",
                             "item_explanation.rml"})
    {
        CAPTURE(name);
        CHECK(Names::IsNotWindow(name));
        CHECK_FALSE(Names::IsFloatingWindow(name));
    }
    CHECK_FALSE(Names::IsNotWindow("inventory.rml"));
    CHECK_FALSE(Names::IsNotWindow("friend_shell.rml"));
}

TEST_CASE("the skill picker, the friend list, letters and chat rooms are floating windows")
{
    for (const char* name : {"mu_helper_skill_picker.rml", "friend_shell.rml", "letter_read.rml", "letter_write.rml",
                             "chat_room.rml"})
    {
        CAPTURE(name);
        CHECK(Names::IsFloatingWindow(name));
        CHECK_FALSE(Names::IsNotWindow(name));
        CHECK_FALSE(Names::IsWorldDialog(name));
    }
    CHECK_FALSE(Names::IsFloatingWindow("mu_helper.rml"));
    CHECK_FALSE(Names::IsFloatingWindow("chat_input.rml"));
    CHECK_FALSE(Names::IsFloatingWindow("chat_log.rml"));
    CHECK_FALSE(Names::IsFloatingWindow(""));
    // A whole name only: a path or a longer name is not one.
    CHECK_FALSE(Names::IsFloatingWindow("friend_shell"));
    CHECK_FALSE(Names::IsFloatingWindow("Data/Interface/RmlUi/friend_shell.rml"));
}

TEST_CASE("only the skill picker is a popup")
{
    CHECK(Names::IsPopupWindow("mu_helper_skill_picker.rml"));
    for (const char* name : {"friend_shell.rml", "letter_read.rml", "letter_write.rml", "chat_room.rml",
                             "options.rml", ""})
    {
        CAPTURE(name);
        CHECK_FALSE(Names::IsPopupWindow(name));
    }
}

TEST_CASE("the reconnect dialog is the world's dialog")
{
    CHECK(Names::IsWorldDialog("reconnect_dialog.rml"));
    CHECK_FALSE(Names::IsWorldDialog("generic_confirm_dialog.rml"));
    CHECK_FALSE(Names::IsWorldDialog("reconnect_dialog"));
    CHECK_FALSE(Names::IsFloatingWindow("reconnect_dialog.rml"));
    CHECK_FALSE(Names::IsNotWindow("reconnect_dialog.rml"));
}
