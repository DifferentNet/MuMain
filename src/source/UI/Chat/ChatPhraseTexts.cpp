#include "stdafx.h"
#include "UI/Chat/ChatPhraseTexts.h"

#include "I18N/All.h"
#include "UI/Chat/ChatPhrases.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <optional>
#include <string>

namespace UI::Chat::PhraseTexts
{
namespace
{
namespace Phrases = UI::Chat::Phrases;

// The slots, not their values: a language switch rewrites what they point at.
const std::array<const wchar_t* const*, Phrases::Count> OwnSlots = {
    &I18N::Game::ChatPhrase01, &I18N::Game::ChatPhrase02, &I18N::Game::ChatPhrase03, &I18N::Game::ChatPhrase04,
    &I18N::Game::ChatPhrase05, &I18N::Game::ChatPhrase06, &I18N::Game::ChatPhrase07, &I18N::Game::ChatPhrase08,
    &I18N::Game::ChatPhrase09, &I18N::Game::ChatPhrase10, &I18N::Game::ChatPhrase11, &I18N::Game::ChatPhrase12,
};
} // namespace

const wchar_t* OwnText(int id)
{
    if (id < 1 || id > Phrases::Count)
        return nullptr;
    return *OwnSlots[static_cast<std::size_t>(id - 1)];
}

bool ShowInOwnLanguage(wchar_t* text, std::size_t capacity)
{
    if (text == nullptr || capacity == 0)
        return false;
    const std::optional<std::wstring> own = Phrases::TryTranslate(text, OwnText);
    if (!own)
        return false;
    const std::size_t length = std::min(own->size(), capacity - 1);
    std::wmemcpy(text, own->c_str(), length);
    text[length] = L'\0';
    return true;
}
} // namespace UI::Chat::PhraseTexts
