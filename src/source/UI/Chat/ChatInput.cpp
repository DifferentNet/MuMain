#include "stdafx.h"
#include "UI/Chat/ChatInput.h"

// Includes mirror ZzzInterface.cpp.
#include "Core/Platform/Imm.h"
#include "UI/Core/UIManager.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/PlayerActionState.h"
#include "Render/Textures/ZzzTexture.h"
#include "Engine/AI/ZzzAI.h"
#include "Engine/Object/ZzzInterface.h"
#include "GameLogic/Combat/ClassAttack.h"
#include "GameLogic/Combat/SkillCast.h"
#include "GameLogic/Combat/SkillExecution.h"
#include "Core/Input/ImeInput.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzOpenData.h"
#include "Render/Effects/ZzzEffect.h"
#include "Scenes/SceneCore.h"
#include "Engine/Pathing/ZzzPath.h"
#include "Audio/DSPlaySound.h"
#include "I18N/All.h"
#include "GameLogic/Events/MatchEvent.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "GameLogic/Quests/CSQuest.h"
#include "GameLogic/Items/CSItemOption.h"
#include "GameLogic/NPCs/npcBreeder.h"
#include "GameLogic/Pets/GIPetManager.h"
#include "Character/CSParts.h"
#include "UI/HUD/UIMapName.h"	// rozy
#include "GameLogic/Events/Cinematic/CDirection.h"
#include "World/MapInfra/MapManager.h"
#include "GameLogic/Events/Event.h"
#include "UI/Core/WindowSystem.h"
#include "GameLogic/Events/w_CursedTemple.h"
#include "GameLogic/Social/PartyManager.h"
#include "UI/Dialogs/CommonMessageBox.h"
#include "GameLogic/Skills/SummonSystem.h"
#include "GameLogic/Skills/SkillManager.h"
#include "World/MapInfra/w_MapHeaders.h"
#include "GameLogic/Combat/DuelMgr.h"
#include "UI/Chat/EmoteKeywords.h"

#include <string_view>
#include <type_traits>

namespace
{
namespace EmoteKeywords = UI::Chat::EmoteKeywords;
namespace Words = I18N::Game;

struct Emote
{
    int action;
    int request;
};

// Plays the emote of the first keyword group the line has a keyword of (UI/Chat/EmoteKeywords.h); whether one did.
bool PlayKeywordEmote(std::wstring_view line)
{
    const wchar_t* const greeting[] = {Words::Hello, Words::EmoteHi, Words::Welcome, Words::Thanks};
    const wchar_t* const goodbye[] = {Words::EnjoyTheGame, Words::Bye, Words::Bye280};
    const wchar_t* const clap[] = {Words::Good, Words::Wow, Words::Nice};
    const wchar_t* const beckon[] = {Words::Here, Words::Come};
    const wchar_t* const point[] = {Words::There, Words::That};
    const wchar_t* const refuse[] = {Words::Not, Words::Never, Words::DoNot, Words::DoNot302};
    const wchar_t* const awkward[] = {L";", Words::Sorry};
    const wchar_t* const cry[] = {L"ㅠ.ㅠ", L"ㅜ.ㅜ", L"T_T", Words::Sad, Words::Cry};
    const wchar_t* const stare[] = {L"ㅡ.ㅡ", L"ㅡ.,ㅡ", L"ㅡ,.ㅡ", L"-.-", L"-_-", Words::Huh, Words::Pooh};
    const wchar_t* const smile[] = {L"^^", L"^.^", L"^_^", Words::Haha, Words::Hehe, Words::Hoho, Words::Hihi};
    const wchar_t* const cheer[] = {Words::Great, Words::OhYeah, Words::OhYeah320, Words::BeatIt};
    const wchar_t* const win[] = {Words::EmoteWin, Words::Victory};
    const wchar_t* const sleep[] = {Words::Sleep, Words::Tired};
    const wchar_t* const cold[] = {Words::Cold, Words::Hurt};
    const wchar_t* const again[] = {Words::Again, Words::EmoteOk};
    const wchar_t* const respect[] = {Words::Respect, Words::Defeated};
    const wchar_t* const salute[] = {Words::Sir, L"/ㅡ", L"ㅡ^"};
    const wchar_t* const rush[] = {Words::Rush, Words::GoGo};
    const wchar_t* const hustle[] = {Words::Hustle, L"hustle"};
    const wchar_t* const provoke[] = {Words::ComeOn};
    const wchar_t* const lookAround[] = {Words::LookAround};

    // In the order the old if-chain tested them: the first match wins.
    const EmoteKeywords::Group groups[] = {
        {greeting}, {goodbye}, {clap},  {beckon}, {point},   {refuse}, {awkward}, {cry},     {stare},
        {smile, true}, {cheer}, {win},  {sleep},  {cold},    {again},  {respect}, {salute},  {rush},
        {hustle},   {provoke}, {lookAround},
    };
    const Emote emotes[] = {
        {PLAYER_GREETING1, AT_GREETING1}, {PLAYER_GOODBYE1, AT_GOODBYE1},   {PLAYER_CLAP1, AT_CLAP1},
        {PLAYER_GESTURE1, AT_GESTURE1},   {PLAYER_DIRECTION1, AT_DIRECTION1}, {PLAYER_UNKNOWN1, AT_UNKNOWN1},
        {PLAYER_AWKWARD1, AT_AWKWARD1},   {PLAYER_CRY1, AT_CRY1},           {PLAYER_SEE1, AT_SEE1},
        {PLAYER_SMILE1, AT_SMILE1},       {PLAYER_CHEER1, AT_CHEER1},       {PLAYER_WIN1, AT_WIN1},
        {PLAYER_SLEEP1, AT_SLEEP1},       {PLAYER_COLD1, AT_COLD1},         {PLAYER_AGAIN1, AT_AGAIN1},
        {PLAYER_RESPECT1, AT_RESPECT1},   {PLAYER_SALUTE1, AT_SALUTE1},     {PLAYER_RUSH1, AT_RUSH1},
        {PLAYER_HUSTLE, AT_HUSTLE},       {PLAYER_PROVOCATION, AT_PROVOCATION}, {PLAYER_LOOK_AROUND, AT_LOOK_AROUND},
    };
    static_assert(std::extent_v<decltype(groups)> == std::extent_v<decltype(emotes)>);

    const int index = EmoteKeywords::FirstMatch(line, groups);
    if (index < 0)
        return false;

    const Emote& emote = emotes[index];
    SetActionClass(Hero, &Hero->Object, emote.action, emote.request);
    SendRequestAction(Hero->Object, static_cast<BYTE>(emote.request));
    return true;
}
} // namespace

namespace UI::Chat
{
void CheckChatText(wchar_t* Text)
{
    CHARACTER* c = Hero;
    OBJECT* o = &c->Object;
    if (PlayKeywordEmote(Text))
        return;

    if (FindText(Text, I18N::Game::TheForehead))
    {
        ITEM* pItem_rr = &CharacterMachine->Equipment[EQUIPMENT_RING_RIGHT];
        ITEM* pItem_rl = &CharacterMachine->Equipment[EQUIPMENT_RING_LEFT];

        if (pItem_rr->Type == ITEM_JACK_OLANTERN_TRANSFORMATION_RING || pItem_rl->Type == ITEM_JACK_OLANTERN_TRANSFORMATION_RING)
        {
            if (rand_fps_check(2))
            {
                SetAction(o, PLAYER_JACK_1);
                SendRequestAction(Hero->Object, AT_JACK1);
            }
            else
            {
                SetAction(o, PLAYER_JACK_2);
                SendRequestAction(Hero->Object, AT_JACK2);
            }

            o->m_iAnimation = 0;
        }
    }
    else if (FindText(Text, I18N::Game::Christmas))
    {
        ITEM* pItem_rr = &CharacterMachine->Equipment[EQUIPMENT_RING_RIGHT];
        ITEM* pItem_rl = &CharacterMachine->Equipment[EQUIPMENT_RING_LEFT];

        if (pItem_rr->Type == ITEM_CHRISTMAS_TRANSFORMATION_RING || pItem_rl->Type == ITEM_CHRISTMAS_TRANSFORMATION_RING)
        {
            if (o->CurrentAction != PLAYER_SANTA_1 && o->CurrentAction != PLAYER_SANTA_2)
            {
                int i = rand() % 3;
                if (rand() % 2)
                {
                    SetAction(o, PLAYER_SANTA_1);
                    SendRequestAction(Hero->Object, AT_SANTA1_1 + i);
                    PlayBuffer(static_cast<ESound>(SOUND_XMAS_JUMP_SANTA + i));
                }
                else
                {
                    SetAction(o, PLAYER_SANTA_2);
                    SendRequestAction(Hero->Object, AT_SANTA2_1 + i);
                    PlayBuffer(SOUND_XMAS_TURN);
                }

                g_XmasEvent->CreateXmasEventEffect(c, o, i);
            }

            o->m_iAnimation = 0;
        }
    }
}
}
