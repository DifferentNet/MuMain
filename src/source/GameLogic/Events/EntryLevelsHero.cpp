#include "stdafx.h"
#include "GameLogic/Events/EntryLevels.h"

#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "I18N/All.h"

namespace GameLogic::Events::EntryLevels
{
bool HeroIsStrongClass()
{
    if (Hero == nullptr)
        return false;
    const CLASS_TYPE baseClass = gCharacterManager.GetBaseClass(Hero->Class);
    return baseClass == CLASS_DARK || baseClass == CLASS_DARK_LORD || baseClass == CLASS_RAGEFIGHTER;
}

int HeroGrade(Event event)
{
    if (CharacterAttribute == nullptr)
        return -1;
    return GradeFor(event, HeroIsStrongClass(), gCharacterManager.IsThirdClass(CharacterAttribute->Class),
        CharacterAttribute->Level);
}

std::wstring BandLabel(Event event, int grade)
{
    const Band band = BandOf(event, HeroIsStrongClass(), grade);
    const bool last = grade == GradeCount(event) - 1;
    const wchar_t* format = event == Event::BloodCastle
        ? (last ? I18N::MiniGameEntry::CastleThirdClass : I18N::Game::CastleDLevelDD)
        : (last ? I18N::MiniGameEntry::SquareThirdClass : I18N::Game::TheDSquareDDLevel);
    wchar_t text[255] = {};
    mu_swprintf(text, format, grade + 1, band.minLevel, band.maxLevel);
    return text;
}
} // namespace GameLogic::Events::EntryLevels
