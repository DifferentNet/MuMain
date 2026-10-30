#include "stdafx.h"
#include "UI/Dialogs/HelpPages.h"

#include "I18N/All.h"

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

void AddTouchControlLines(std::vector<UI::Help::PageLine>& lines)
{
    const wchar_t* const touchLines[] = {
        I18N::Game::TouchHelpWorld,   I18N::Game::TouchHelpStick,  I18N::Game::TouchHelpAttack,
        I18N::Game::TouchHelpSkills,  I18N::Game::TouchHelpPickUp, I18N::Game::TouchHelpAuto,
        I18N::Game::TouchHelpPotions, I18N::Game::TouchHelpMenu,   I18N::Game::TouchHelpItem,
        I18N::Game::TouchHelpDrag,    I18N::Game::TouchHelpView,
    };
    for (const wchar_t* line : touchLines)
        AddText(lines, line);
}
} // namespace

std::vector<UI::Help::PageLine> UI::Help::BuildPage(int page, bool touchControls)
{
    std::vector<PageLine> lines;
    AddHalfSpacer(lines);

    if (page == KeyFunctionPage && touchControls)
    {
        AddText(lines, I18N::Game::TouchHelpTitle, true);
        AddHalfSpacer(lines);
        AddTouchControlLines(lines);
    }
    else if (page == KeyFunctionPage)
    {
        AddText(lines, I18N::Game::KeyFunction, true);
        AddHalfSpacer(lines);
        AddKeyFunctionLines(lines);
    }
    else
    {
        AddText(lines, I18N::Game::ChattingInstructions, true);
        AddHalfSpacer(lines);
        for (int i = 0; i < kChattingTextCount; ++i)
            AddText(lines, I18N::Game::Lookup(kFirstChattingText + i));
    }

    AddHalfSpacer(lines);
    return lines;
}
