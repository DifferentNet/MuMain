#include "stdafx.h"
#include "UI/Character/OverheadNames.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "App/Platform/Windows/Winmain.h"
#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "Engine/Object/ZzzObject.h"
#include "GameLogic/Events/Titles.h"
#include "GameLogic/Items/PersonalShopTitleImp.h"
#include "Guild/GuildTypes.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/Character/TitlePlates.h"
#include "UI/Chat/Chat.h"
#include "World/MapInfra/MapManager.h"

namespace UI::OverheadNames
{
namespace
{
struct Candidate
{
    CHARACTER* character;
    float distance;
    int x;
    int y;
};

bool IsGameMaster(CHARACTER* c)
{
    return g_isCharacterBuff((&c->Object), eBuff_GMEffect) || c->CtlCode == CTLCODE_20OPERATOR ||
           c->CtlCode == CTLCODE_08OPERATOR;
}

// The hover plate's colours (Chat.cpp SetPlayerColor and RenderBoolean), so a name keeps its
// colour when the plate takes over.
void SetNameColor(CHARACTER* c, bool gameMaster)
{
    if (c == Hero)
    {
        g_pRenderText->SetTextColor(200, 255, 0, 255);
        return;
    }
    if (gameMaster)
    {
        g_pRenderText->SetTextColor(100, 250, 250, 255);
        return;
    }
    if (c->Object.Kind == KIND_NPC)
    {
        g_pRenderText->SetTextColor(150, 255, 240, 255);
        return;
    }
    UI::Chat::SetPlayerColor(c->PK);
}

bool IsShown(CHARACTER* c, bool players, bool npcs)
{
    OBJECT* o = &c->Object;
    if (!o->Live || !o->Visible || c->ID[0] == L'\0' || c->Dead > 0)
        return false;

    if (o->Kind == KIND_PLAYER)
    {
        if (!players)
            return false;
    }
    else if (o->Kind == KIND_NPC)
    {
        if (!npcs)
            return false;
    }
    else
    {
        return false;
    }

    if (g_isCharacterBuff(o, eBuff_CrywolfNPCHide))
        return false;
    if (c != Hero && g_isCharacterBuff(o, eBuff_Cloaking))
        return false;

    if (o->Kind == KIND_PLAYER)
    {
        // An open personal shop shows its own title there.
        if (IsShopTitleVisible(c))
            return false;
        // The hover plate hides other gens' names on the strife maps too.
        if (c != Hero && IsStrifeMap(gMapManager.WorldActive) && Hero->m_byGensInfluence != c->m_byGensInfluence)
            return false;
    }

    // Hovered or speaking: the plate is up and says more.
    return !UI::Chat::HasPlate(c);
}

void HeadPosition(OBJECT* o, vec3_t out)
{
    float lift = 60.f;
    if ((o->Type >= MODEL_LITTLESANTA && o->Type <= MODEL_LITTLESANTA_END) || o->Type == MODEL_MERCHANT_MAN)
        lift = 20.f;
    Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + lift, out);
}
} // namespace

void Render()
{
    const GameConfig& config = GameConfig::GetInstance();
    const bool names = config.GetOverheadPlayerNames();
    const bool npcs = config.GetOverheadNpcNames();
    // The titles go over the names, or over the heads when the names are off.
    const bool titles = UI::TitlePlates::Enabled() && GameLogic::Titles::Titles().Count() > 0;
    const bool players = names || titles;
    if ((!players && !npcs) || Hero == nullptr)
        return;

    // Chaos Castle hides who is who; the hover plate is off there too.
    if (gMapManager.InChaosCastle())
        return;

    std::array<Candidate, MAX_CHARACTERS_CLIENT> found{};
    int count = 0;
    const OBJECT* hero = &Hero->Object;

    for (int i = 0; i < MAX_CHARACTERS_CLIENT; ++i)
    {
        CHARACTER* c = &CharactersClient[i];
        if (!IsShown(c, players, npcs))
            continue;

        OBJECT* o = &c->Object;
        vec3_t head;
        HeadPosition(o, head);

        vec3_t view;
        VectorTransform(head, g_Camera.Matrix, view);
        if (view[2] >= 0)
            continue; // behind the camera

        int x = 0;
        int y = 0;
        CameraProjection::WorldToScreen(g_Camera, head, &x, &y);
        if (x < -60 || y < -20 || x > REFERENCE_WIDTH + 60 || y > REFERENCE_HEIGHT + 20)
            continue;

        const float dx = o->Position[0] - hero->Position[0];
        const float dy = o->Position[1] - hero->Position[1];
        found[count++] = Candidate{ c, dx * dx + dy * dy, x, y };
    }

    if (count == 0)
        return;

    const int shown = std::min(count, MaxNames);
    std::partial_sort(found.begin(), found.begin() + shown, found.begin() + count,
                      [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    wchar_t guild[64];
    // Farthest first, so the nearest names end up on top.
    for (int n = shown - 1; n >= 0; --n)
    {
        CHARACTER* c = found[n].character;
        const bool player = c->Object.Kind == KIND_PLAYER;
        const GameLogic::Titles::Worn* title = player && titles ? GameLogic::Titles::Titles().Of(c->ID) : nullptr;
        if (player && !names)
        {
            if (title != nullptr)
                UI::TitlePlates::Render(*title, *c, found[n].x, found[n].y);
            continue;
        }

        const bool gameMaster = player && IsGameMaster(c);

        guild[0] = L'\0';
        if (c->Object.Kind == KIND_PLAYER && c->GuildMarkIndex >= 0 && GuildMark[c->GuildMarkIndex].GuildName[0])
            mu_swprintf(guild, L"[%ls] ", GuildMark[c->GuildMarkIndex].GuildName);

        g_pRenderText->SetFont(g_hFont);
        const SIZE guildSize = guild[0] ? g_pRenderText->MeasureText(guild, lstrlen(guild)) : SIZE{ 0, 0 };
        g_pRenderText->SetFont(gameMaster ? g_hFontBold : g_hFont);
        SIZE nameSize = g_pRenderText->MeasureText(c->ID, lstrlen(c->ID));
        nameSize.cx += 3;

        const int lineHeight = std::max<int>(guildSize.cy, nameSize.cy);
        const int width = guildSize.cx + nameSize.cx;
        int x = found[n].x - width / 2;
        const int y = found[n].y - lineHeight;

        if (gameMaster)
            g_pRenderText->SetBgColor(30, 30, 30, 150);
        else
            g_pRenderText->SetBgColor(0, 0, 0, 110);

        if (guild[0])
        {
            g_pRenderText->SetFont(g_hFont);
            if (c->GuildMarkIndex == Hero->GuildMarkIndex)
                g_pRenderText->SetTextColor(GetGuildRelationShipTextColor(GR_UNION));
            else
                g_pRenderText->SetTextColor(GetGuildRelationShipTextColor(c->GuildRelationShip));
            g_pRenderText->RenderText(x, y, guild, guildSize.cx, lineHeight, RT3_SORT_LEFT);
            x += guildSize.cx;
        }

        g_pRenderText->SetFont(gameMaster ? g_hFontBold : g_hFont);
        SetNameColor(c, gameMaster);
        g_pRenderText->RenderText(x, y, c->ID, nameSize.cx, lineHeight, RT3_SORT_LEFT);

        if (title != nullptr)
            UI::TitlePlates::Render(*title, *c, found[n].x, y - 1);
    }

    g_pRenderText->SetFont(g_hFont);
}
} // namespace UI::OverheadNames
