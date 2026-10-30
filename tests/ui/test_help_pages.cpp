#include "App/stdafx.h"

#include <doctest.h>

#include <algorithm>

#include "I18N/All.h"
#include "UI/Dialogs/HelpPages.h"

namespace
{
using UI::Help::Page;

constexpr UI::Help::Devices kTouch{true, false};
constexpr UI::Help::Devices kPad{false, true};
constexpr UI::Help::Devices kTouchAndPad{true, true};

// The chat page's index among the devices' pages.
int ChattingIndex(UI::Help::Devices devices)
{
    const std::vector<Page> pages = UI::Help::Pages(devices);
    return static_cast<int>(std::find(pages.begin(), pages.end(), Page::Chatting) - pages.begin());
}

int LastIndex(UI::Help::Devices devices)
{
    return static_cast<int>(UI::Help::Pages(devices).size()) - 1;
}

long CountHeadings(const std::vector<UI::Help::PageLine>& lines)
{
    return std::count_if(lines.begin(), lines.end(), [](const UI::Help::PageLine& line) { return line.heading; });
}

long CountHints(const std::vector<UI::Help::PageLine>& lines)
{
    return std::count_if(lines.begin(), lines.end(), [](const UI::Help::PageLine& line) { return line.hint; });
}

// The hint row is the last but one; the page ends with a spacer.
const UI::Help::PageLine& Hint(const std::vector<UI::Help::PageLine>& lines)
{
    return lines[lines.size() - 2];
}
} // namespace

TEST_CASE("help pages: the devices in hand pick the first pages, then the chat page [ui][help]")
{
    CHECK(UI::Help::Pages({}) == std::vector<Page>{Page::Keys, Page::Chatting});
    // The chat rows and the commands together run past a phone's screen: the commands follow.
    CHECK(UI::Help::Pages(kTouch) == std::vector<Page>{Page::Touch, Page::Chatting, Page::Commands});
    CHECK(UI::Help::Pages(kPad) == std::vector<Page>{Page::Gamepad, Page::Chatting, Page::Commands});
    // Both on one page were too tall for a phone: each has its own.
    CHECK(UI::Help::Pages(kTouchAndPad) ==
          std::vector<Page>{Page::Touch, Page::Gamepad, Page::Chatting, Page::Commands});
}

TEST_CASE("help key page: heading, F1-F4, engine hotkeys, the shipped keys, then MU Skynet's [ui][help]")
{
    const auto lines = UI::Help::BuildPage(0, {});

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
    const auto lines = UI::Help::BuildPage(1, {});

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
    const auto lines = UI::Help::BuildPage(0, kTouch);

    // Spacer, heading, spacer, 16 touch rows, spacer, the tap hint, spacer.
    REQUIRE(lines.size() == 22);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::TouchHelpTitle);
    CHECK(CountHeadings(lines) == 1);
    CHECK(CountHints(lines) == 1);
    CHECK(lines[3].text == I18N::Game::TouchHelpWorld);
    CHECK(lines[5].text == I18N::Game::HelpControlsWalkHit);
    CHECK(lines[7].text == I18N::Game::TouchHelpAttack);
    CHECK(lines[12].text == I18N::Game::TouchHelpPickUp);
    CHECK(lines[18].text == I18N::Game::TouchHelpView);
    CHECK(Hint(lines).hint);
    CHECK(Hint(lines).text == I18N::Game::TouchHelpNextHint);
    CHECK(lines.back().halfSpacer);

    // Spacer, heading, spacer, the 16 chat rows, spacer, the tap hint, spacer: the commands follow.
    const auto chat = UI::Help::BuildPage(1, kTouch);
    REQUIRE(chat.size() == 22);
    CHECK(chat[1].text == I18N::Game::ChattingInstructions);
    CHECK(chat[18].text == I18N::Game::Lookup(156));
    CHECK(CountHeadings(chat) == 1);
    CHECK(Hint(chat).text == I18N::Game::TouchHelpNextHint);
}

TEST_CASE("help commands page: MU Skynet's commands after the chat page, the last page [ui][help]")
{
    const auto lines = UI::Help::BuildPage(2, kTouch);

    // Spacer, heading, spacer, 11 command rows, spacer, the tap hint, spacer.
    REQUIRE(lines.size() == 17);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::HelpSkynetCommandsHeading);
    CHECK(lines[2].halfSpacer);
    CHECK(CountHeadings(lines) == 1);
    CHECK(CountHints(lines) == 1);
    CHECK(lines[3].text == I18N::Game::HelpSkynetCommandList);
    CHECK(lines[13].text == I18N::Game::HelpSkynetCommandStuck);
    CHECK(Hint(lines).text == I18N::Game::TouchHelpCloseHint);
    CHECK(lines.back().halfSpacer);

    CHECK(UI::Help::BuildPage(2, kPad)[1].text == I18N::Game::HelpSkynetCommandsHeading);
    CHECK(Hint(UI::Help::BuildPage(2, kPad)).text == I18N::Game::PadHelpPagesHint);
}

TEST_CASE("help with a gamepad: the pad's buttons instead of the keys, LB and RB turn the pages [ui][help]")
{
    const auto lines = UI::Help::BuildPage(0, kPad);

    // Spacer, heading, spacer, 23 pad rows, spacer, the LB/RB hint, spacer.
    REQUIRE(lines.size() == 29);
    CHECK(lines[1].heading);
    CHECK(lines[1].text == I18N::Game::PadHelpTitle);
    CHECK(CountHeadings(lines) == 1);
    CHECK(CountHints(lines) == 1);
    CHECK(lines[3].text == I18N::Game::PadHelpSticks);
    CHECK(lines[6].text == I18N::Game::PadHelpAttack);
    CHECK(lines[12].text == I18N::Game::PadHelpSkillList);
    CHECK(lines[21].text == I18N::Game::PadHelpKeyboard);
    CHECK(lines[25].text == I18N::Game::PadHelpScreensKeys);
    CHECK(Hint(lines).text == I18N::Game::PadHelpPagesHint);

    CHECK(Hint(UI::Help::BuildPage(1, kPad)).text == I18N::Game::PadHelpPagesHint);
}

TEST_CASE("help with touch and a gamepad: the touch page, the pad page, then chat and commands [ui][help]")
{
    CHECK(UI::Help::BuildPage(0, kTouchAndPad)[1].text == I18N::Game::TouchHelpTitle);
    CHECK(UI::Help::BuildPage(1, kTouchAndPad)[1].text == I18N::Game::PadHelpTitle);
    CHECK(UI::Help::BuildPage(2, kTouchAndPad)[1].text == I18N::Game::ChattingInstructions);
    CHECK(UI::Help::BuildPage(3, kTouchAndPad)[1].text == I18N::Game::HelpSkynetCommandsHeading);
    // The pad in hand turns the pages: its hint on every page.
    CHECK(Hint(UI::Help::BuildPage(0, kTouchAndPad)).text == I18N::Game::PadHelpPagesHint);
    CHECK(Hint(UI::Help::BuildPage(3, kTouchAndPad)).text == I18N::Game::PadHelpPagesHint);
}

TEST_CASE("help page index: before the first, the first; past the last, the last [ui][help]")
{
    CHECK(UI::Help::BuildPage(-1, {})[1].text == I18N::Game::KeyFunction);
    CHECK(ChattingIndex({}) == LastIndex({}));
    CHECK(UI::Help::BuildPage(LastIndex({}) + 1, {})[1].text == I18N::Game::ChattingInstructions);
    CHECK(ChattingIndex(kTouchAndPad) == 2);
    CHECK(UI::Help::BuildPage(LastIndex(kTouchAndPad) + 5, kTouchAndPad)[1].text ==
          I18N::Game::HelpSkynetCommandsHeading);
}
