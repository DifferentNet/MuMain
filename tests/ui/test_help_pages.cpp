#include "App/stdafx.h"

#include <doctest.h>

#include <algorithm>

#include "I18N/All.h"
#include "UI/Dialogs/HelpPages.h"

namespace
{
constexpr UI::Help::Devices kTouch{true, false};
constexpr UI::Help::Devices kPad{false, true};
constexpr UI::Help::Devices kTouchAndPad{true, true};

long CountHeadings(const std::vector<UI::Help::PageLine>& lines)
{
    return std::count_if(lines.begin(), lines.end(), [](const UI::Help::PageLine& line) { return line.heading; });
}

long CountHints(const std::vector<UI::Help::PageLine>& lines)
{
    return std::count_if(lines.begin(), lines.end(), [](const UI::Help::PageLine& line) { return line.hint; });
}
} // namespace

TEST_CASE("help key page: heading, F1-F4, engine hotkeys, the shipped keys, then MU Skynet's [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::KeyFunctionPage, {});

    // Spacer, heading, spacer, 4 + 6 + 15 key rows (the original's TextList), then spacer, MU Skynet
    // heading, 5 key rows, spacer, the F1 hint, spacer.
    REQUIRE(lines.size() == 38);
    CHECK(lines.front().halfSpacer);
    CHECK(lines[2].halfSpacer);
    CHECK(lines.back().halfSpacer);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::KeyFunction);
    CHECK(CountHeadings(lines) == 2);
    CHECK(CountHints(lines) == 1);

    CHECK(lines[3].text == I18N::Game::Lookup(121));
    CHECK(lines[6].text == I18N::Game::Lookup(124));
    CHECK(lines[7].text == I18N::Game::F8ToggleMonsterHPBar);
    CHECK(lines[12].text == I18N::Game::JToggleChatCommands);
    CHECK(lines[13].text == I18N::Game::Lookup(125));
    CHECK(lines[27].text == I18N::Game::Lookup(139));

    CHECK(lines[28].halfSpacer);
    CHECK(lines[29].heading);
    CHECK(lines[29].text == I18N::Game::HelpSkynetKeysHeading);
    CHECK(lines[30].text == I18N::Game::HelpSkynetKeysEvents);
    CHECK(lines[34].text == I18N::Game::HelpSkynetKeysCommand);
    CHECK(lines[35].halfSpacer);
    CHECK(lines[36].hint);
    CHECK(lines[36].text == I18N::Game::HelpNextPageHint);
}

TEST_CASE("help chat page: heading, the sixteen chat rows, then MU Skynet's commands [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::ChattingPage, {});

    // Spacer, heading, spacer, 16 chat rows, then spacer, commands heading, 11 command rows, spacer,
    // the F1 hint, spacer.
    REQUIRE(lines.size() == 35);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::ChattingInstructions);
    CHECK(CountHeadings(lines) == 2);
    CHECK(CountHints(lines) == 1);
    CHECK(lines[3].text == I18N::Game::Lookup(141));
    CHECK(lines[18].text == I18N::Game::Lookup(156));
    CHECK(lines[20].heading);
    CHECK(lines[20].text == I18N::Game::HelpSkynetCommandsHeading);
    CHECK(lines[21].text == I18N::Game::HelpSkynetCommandList);
    CHECK(lines[31].text == I18N::Game::HelpSkynetCommandStuck);
    CHECK(lines[33].hint);
    CHECK(lines[33].text == I18N::Game::HelpCloseHint);
    CHECK(lines.back().halfSpacer);
    CHECK(lines.back().text.empty());
}

TEST_CASE("help with touch controls: the touch page instead of the keys, the same chat page [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::KeyFunctionPage, kTouch);

    // Spacer, heading, spacer, 11 touch rows, spacer.
    REQUIRE(lines.size() == 15);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::TouchHelpTitle);
    CHECK(CountHeadings(lines) == 1);
    CHECK(lines[3].text == I18N::Game::TouchHelpWorld);
    CHECK(lines[13].text == I18N::Game::TouchHelpView);
    CHECK(lines.back().halfSpacer);

    CHECK(UI::Help::BuildPage(UI::Help::ChattingPage, kTouch).size() ==
          UI::Help::BuildPage(UI::Help::ChattingPage, {}).size());
}

TEST_CASE("help with a gamepad: the pad's buttons instead of the keys [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::KeyFunctionPage, kPad);

    // Spacer, heading, spacer, 11 pad rows, spacer.
    REQUIRE(lines.size() == 15);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::PadHelpTitle);
    CHECK(CountHeadings(lines) == 1);
    CHECK(lines[3].text == I18N::Game::PadHelpSticks);
    CHECK(lines[12].text == I18N::Game::PadHelpKeyboard);
    CHECK(lines[13].text == I18N::Game::PadHelpScreens);
    CHECK(lines.back().halfSpacer);
}

TEST_CASE("help with touch and a gamepad: the touch section, then the pad section [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::KeyFunctionPage, kTouchAndPad);

    // The touch page without its closing spacer, a spacer, the pad heading, a spacer, 11 rows, a spacer.
    REQUIRE(lines.size() == 29);
    CHECK(CountHeadings(lines) == 2);
    CHECK(lines[1].text == I18N::Game::TouchHelpTitle);
    CHECK(lines[13].text == I18N::Game::TouchHelpView);
    CHECK(lines[14].halfSpacer);
    CHECK(lines[15].heading);
    CHECK(lines[15].text == I18N::Game::PadHelpTitle);
    CHECK(lines[17].text == I18N::Game::PadHelpSticks);
    CHECK(lines.back().halfSpacer);

    CHECK(UI::Help::BuildPage(UI::Help::ChattingPage, kPad).size() ==
          UI::Help::BuildPage(UI::Help::ChattingPage, {}).size());
}
