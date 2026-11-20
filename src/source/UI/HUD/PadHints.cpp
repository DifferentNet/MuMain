#include "stdafx.h"
#include "UI/HUD/PadHints.h"

#include "Data/GameConfig/GameConfig.h"
#include "I18N/All.h"
#include "UI/Chat/ChatMessages.h"
#include "UI/HUD/PadGlyphs.h"
#include "UI/Mobile/InterfaceMode.h"

namespace UI::HUD::PadHints
{
namespace
{
const wchar_t* TextOf(Hint hint)
{
    switch (hint)
    {
    case Hint::PickUp:
        return I18N::Game::PadHintPickUp;
    case Hint::Window:
        return I18N::Game::PadHintWindow;
    case Hint::BagFull:
        return I18N::Game::PadHintBagFull;
    case Hint::Death:
        return I18N::Game::PadHintDeath;
    case Hint::SkillsThird:
        return I18N::Game::PadHintSkillsThird;
    case Hint::SmallScreen:
        return I18N::Game::PadHintSmallScreen;
    }
    return nullptr;
}
} // namespace

void Show(Hint hint)
{
    if (!UI::Mobile::IsGamepadInUse())
        return;
    GameConfig& config = GameConfig::GetInstance();
    const int seen = config.GetGamepadHintsSeen();
    if (Seen(seen, hint))
        return;
    const wchar_t* text = TextOf(hint);
    if (text == nullptr)
        return;
    config.SetGamepadHintsSeen(Marked(seen, hint));
    config.Save();
    UI::Chat::PostSystem(UI::HUD::PadGlyphs::Named(text), mu::ui::window::TYPE_SYSTEM_MESSAGE);
}
} // namespace UI::HUD::PadHints
