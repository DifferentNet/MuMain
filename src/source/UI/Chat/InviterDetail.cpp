#include "stdafx.h"
#include "UI/Chat/InviterDetail.h"

#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Guild/GuildCache.h"
#include "I18N/All.h"
#include "UI/Chat/InviterDetailText.h"

namespace UI::Chat::InviterDetail
{
namespace
{
std::wstring ForCharacter(const CHARACTER* c)
{
    if (c == nullptr || c->Object.Kind != KIND_PLAYER)
        return {};

    const wchar_t* guildName = L"";
    if (c->GuildMarkIndex >= 0 && c->GuildMarkIndex < MAX_MARKS)
        guildName = GuildMark[c->GuildMarkIndex].GuildName;
    return Compose(gCharacterManager.GetCharacterClassText(c->Class), I18N::Game::TouchMenuGuild, guildName);
}
} // namespace

std::wstring ForKey(int characterKey)
{
    return ForCharacter(FindCharacterByKey(characterKey));
}

std::wstring ForName(const wchar_t* name)
{
    if (name == nullptr || name[0] == L'\0')
        return {};

    for (int i = 0; i < CharacterSlotEnd(); ++i)
    {
        const CHARACTER* c = &CharactersClient[i];
        if (c->Object.Live && wcsncmp(c->ID, name, MAX_USERNAME_SIZE) == 0)
            return ForCharacter(c);
    }
    return {};
}
} // namespace UI::Chat::InviterDetail
