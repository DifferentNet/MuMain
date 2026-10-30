#include "stdafx.h"
#include "UI/Dialogs/HelpPages.h"

#include "I18N/All.h"

#include <algorithm>

namespace
{
constexpr int kFirstFunctionKeyText = 121; // F1..F4, then the rest of the shipped key list
constexpr int kFunctionKeyTextsBeforeExtras = 4;
constexpr int kShippedKeyTextCount = 19;
constexpr int kFirstChattingText = 141;
constexpr int kChattingTextCount = 16;

void AddHalfSpacer(std::vector<UI::Help::PageLine>& lines)
{
    UI::Help::PageLine line;
    line.halfSpacer = true;
    lines.push_back(std::move(line));
}

void AddText(std::vector<UI::Help::PageLine>& lines, const wchar_t* text, bool heading = false)
{
    UI::Help::PageLine line;
    line.text = text;
    line.heading = heading;
    lines.push_back(std::move(line));
}

void AddHint(std::vector<UI::Help::PageLine>& lines, const wchar_t* text)
{
    AddHalfSpacer(lines);
    UI::Help::PageLine line;
    line.text = text;
    line.hint = true;
    lines.push_back(std::move(line));
}

// MU Skynet's windows and the keys the original list left out. Two or three keys a row: the key
// page has to stay above the bottom bar on a 1024x768 screen.
void AddSkynetKeyLines(std::vector<UI::Help::PageLine>& lines)
{
    AddHalfSpacer(lines);
    AddText(lines, I18N::Game::HelpSkynetKeysHeading, true);

    const wchar_t* const keyLines[] = {
        I18N::Game::HelpSkynetKeysEvents, I18N::Game::HelpSkynetKeysPass,    I18N::Game::HelpSkynetKeysMap,
        I18N::Game::HelpSkynetKeysHelper, I18N::Game::HelpSkynetKeysCommand,
    };
    for (const wchar_t* line : keyLines)
        AddText(lines, line);
}

// The server commands players use most; J lists all of them with their descriptions.
void AddSkynetCommandRows(std::vector<UI::Help::PageLine>& lines)
{
    const wchar_t* const commandLines[] = {
        I18N::Game::HelpSkynetCommandList,       I18N::Game::HelpSkynetCommandReset,
        I18N::Game::HelpSkynetCommandResetStats, I18N::Game::HelpSkynetCommandAddPoints,
        I18N::Game::HelpSkynetCommandCp,         I18N::Game::HelpSkynetCommandEvents,
        I18N::Game::HelpSkynetCommandPass,       I18N::Game::HelpSkynetCommandPvp,
        I18N::Game::HelpSkynetCommandDuels,      I18N::Game::HelpSkynetCommandTier,
        I18N::Game::HelpSkynetCommandStuck,
    };
    for (const wchar_t* line : commandLines)
        AddText(lines, line);
}

// The commands under the chat page's rows, on the key page's screen.
void AddSkynetCommandLines(std::vector<UI::Help::PageLine>& lines)
{
    AddHalfSpacer(lines);
    AddText(lines, I18N::Game::HelpSkynetCommandsHeading, true);
    AddSkynetCommandRows(lines);
}

// The commands on a page of their own, after the chat page (UI::Help::Pages()).
void AddCommandsPage(std::vector<UI::Help::PageLine>& lines)
{
    AddText(lines, I18N::Game::HelpSkynetCommandsHeading, true);
    AddHalfSpacer(lines);
    AddSkynetCommandRows(lines);
}

void AddKeyFunctionLines(std::vector<UI::Help::PageLine>& lines)
{
    for (int i = 0; i < kFunctionKeyTextsBeforeExtras; ++i)
        AddText(lines, I18N::Game::Lookup(kFirstFunctionKeyText + i));

    // Engine-added camera/MU Helper hotkey entries, inserted between F4 and the shipped entries.
    const wchar_t* const extraHelpLines[] = {
        I18N::Game::F8ToggleMonsterHPBar, I18N::Game::F9Toggle3DCamera,   I18N::Game::F10LockUnlockCameraZoom,
        I18N::Game::F11ResetCameraView,   I18N::Game::HomeToggleMUHelper, I18N::Game::JToggleChatCommands,
    };
    for (const wchar_t* line : extraHelpLines)
        AddText(lines, line);

    for (int i = kFunctionKeyTextsBeforeExtras; i < kShippedKeyTextCount; ++i)
        AddText(lines, I18N::Game::Lookup(kFirstFunctionKeyText + i));
}

void AddKeysPage(std::vector<UI::Help::PageLine>& lines)
{
    AddText(lines, I18N::Game::KeyFunction, true);
    AddHalfSpacer(lines);
    AddKeyFunctionLines(lines);
    AddSkynetKeyLines(lines);
}

// The touch controls as they play after the 2026-10-30 round: a tap acts once, a hold goes on.
void AddTouchPage(std::vector<UI::Help::PageLine>& lines)
{
    AddText(lines, I18N::Game::TouchHelpTitle, true);
    AddHalfSpacer(lines);
    const wchar_t* const touchLines[] = {
        I18N::Game::TouchHelpWorld,     I18N::Game::TouchHelpStick,         I18N::Game::HelpControlsWalkHit,
        I18N::Game::TouchHelpStickGate, I18N::Game::TouchHelpAttack,        I18N::Game::HelpControlsAutoAttack,
        I18N::Game::TouchHelpPk,        I18N::Game::TouchHelpSkills,        I18N::Game::HelpControlsSkillsSelf,
        I18N::Game::TouchHelpSkillsSet, I18N::Game::TouchHelpParty,         I18N::Game::TouchHelpPickUp,
        I18N::Game::TouchHelpAuto,      I18N::Game::TouchHelpPotions,       I18N::Game::TouchHelpMenu,
        I18N::Game::TouchHelpItem,      I18N::Game::TouchHelpDrag,          I18N::Game::TouchHelpView,
    };
    for (const wchar_t* line : touchLines)
        AddText(lines, line);
}

void AddGamepadPage(std::vector<UI::Help::PageLine>& lines)
{
    AddText(lines, I18N::Game::PadHelpTitle, true);
    AddHalfSpacer(lines);
    const wchar_t* const padLines[] = {
        I18N::Game::PadHelpSticks,
        I18N::Game::HelpControlsWalkHit,
        I18N::Game::PadHelpStickGate,
        I18N::Game::PadHelpAttack,
        I18N::Game::HelpControlsAutoAttack,
        I18N::Game::PadHelpPk,
        I18N::Game::PadHelpClickA,
        I18N::Game::PadHelpSkills,
        I18N::Game::PadHelpSkillsUse,
        I18N::Game::HelpControlsSkillsSelf,
        I18N::Game::PadHelpSkillList,
        I18N::Game::PadHelpBack,
        I18N::Game::PadHelpPickUp,
        I18N::Game::PadHelpAuto,
        I18N::Game::PadHelpPotions,
        I18N::Game::PadHelpClick,
        I18N::Game::PadHelpRightClick,
        I18N::Game::PadHelpMenu,
        I18N::Game::PadHelpDialog,
        I18N::Game::PadHelpKeyboard,
        I18N::Game::PadHelpKeyboardKeys,
        I18N::Game::PadHelpKeyboardMore,
        I18N::Game::PadHelpScreens,
        I18N::Game::PadHelpScreensKeys,
    };
    for (const wchar_t* line : padLines)
        AddText(lines, line);
}

// With touch controls or a gamepad in hand the commands follow on a page of their own: the chat rows
// and the commands together run past the bottom of a phone's screen, hint row and all.
bool CommandsOnTheirOwnPage(UI::Help::Devices devices)
{
    return devices.touchControls || devices.gamepad;
}

void AddChattingPage(std::vector<UI::Help::PageLine>& lines, UI::Help::Devices devices)
{
    AddText(lines, I18N::Game::ChattingInstructions, true);
    AddHalfSpacer(lines);
    for (int i = 0; i < kChattingTextCount; ++i)
        AddText(lines, I18N::Game::Lookup(kFirstChattingText + i));
    if (!CommandsOnTheirOwnPage(devices))
        AddSkynetCommandLines(lines);
}

void AddPageLines(std::vector<UI::Help::PageLine>& lines, UI::Help::Page page, UI::Help::Devices devices)
{
    switch (page)
    {
    case UI::Help::Page::Keys:
        AddKeysPage(lines);
        break;
    case UI::Help::Page::Touch:
        AddTouchPage(lines);
        break;
    case UI::Help::Page::Gamepad:
        AddGamepadPage(lines);
        break;
    case UI::Help::Page::Chatting:
        AddChattingPage(lines, devices);
        break;
    case UI::Help::Page::Commands:
        AddCommandsPage(lines);
        break;
    }
}

// What turns the page with the device in hand: the pad's LB and RB, a tap on the hint row, F1.
const wchar_t* PageHint(UI::Help::Devices devices, bool lastPage)
{
    if (devices.gamepad)
        return I18N::Game::PadHelpPagesHint;
    if (devices.touchControls)
        return lastPage ? I18N::Game::TouchHelpCloseHint : I18N::Game::TouchHelpNextHint;
    return lastPage ? I18N::Game::HelpCloseHint : I18N::Game::HelpNextPageHint;
}
} // namespace

std::vector<UI::Help::Page> UI::Help::Pages(Devices devices)
{
    std::vector<Page> pages;
    if (devices.touchControls)
        pages.push_back(Page::Touch);
    if (devices.gamepad)
        pages.push_back(Page::Gamepad);
    if (pages.empty())
        pages.push_back(Page::Keys);
    pages.push_back(Page::Chatting);
    if (CommandsOnTheirOwnPage(devices))
        pages.push_back(Page::Commands);
    return pages;
}

std::vector<UI::Help::PageLine> UI::Help::BuildPage(int index, Devices devices)
{
    const std::vector<Page> pages = Pages(devices);
    const int last = static_cast<int>(pages.size()) - 1;
    const int page = std::clamp(index, 0, last);

    std::vector<PageLine> lines;
    AddHalfSpacer(lines);
    AddPageLines(lines, pages[page], devices);
    AddHint(lines, PageHint(devices, page == last));
    AddHalfSpacer(lines);
    return lines;
}
