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
} // namespace

TEST_CASE("help key page: heading, F1-F4, engine hotkeys, then the shipped keys [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::KeyFunctionPage, {});

    // Spacer, heading, spacer, 4 + 6 + 15 key rows, spacer -- the original's TextList.
    REQUIRE(lines.size() == 29);
    CHECK(lines.front().halfSpacer);
    CHECK(lines[2].halfSpacer);
    CHECK(lines.back().halfSpacer);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::KeyFunction);
    CHECK(CountHeadings(lines) == 1);

    CHECK(lines[3].text == I18N::Game::Lookup(121));
    CHECK(lines[6].text == I18N::Game::Lookup(124));
    CHECK(lines[7].text == I18N::Game::F8ToggleMonsterHPBar);
    CHECK(lines[12].text == I18N::Game::JToggleChatCommands);
    CHECK(lines[13].text == I18N::Game::Lookup(125));
    CHECK(lines[27].text == I18N::Game::Lookup(139));
}

TEST_CASE("help chat page: heading and the sixteen chat rows [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::ChattingPage, {});

    REQUIRE(lines.size() == 20);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::ChattingInstructions);
    CHECK(CountHeadings(lines) == 1);
    CHECK(lines[3].text == I18N::Game::Lookup(141));
    CHECK(lines[18].text == I18N::Game::Lookup(156));
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

    // Spacer, heading, spacer, 10 pad rows, spacer.
    REQUIRE(lines.size() == 14);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::PadHelpTitle);
    CHECK(CountHeadings(lines) == 1);
    CHECK(lines[3].text == I18N::Game::PadHelpSticks);
    CHECK(lines[12].text == I18N::Game::PadHelpScreens);
    CHECK(lines.back().halfSpacer);
}

TEST_CASE("help with touch and a gamepad: the touch section, then the pad section [ui][help]")
{
    const auto lines = UI::Help::BuildPage(UI::Help::KeyFunctionPage, kTouchAndPad);

    // The touch page without its closing spacer, a spacer, the pad heading, a spacer, 10 rows, a spacer.
    REQUIRE(lines.size() == 28);
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
