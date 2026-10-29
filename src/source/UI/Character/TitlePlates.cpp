#include "stdafx.h"
#include "UI/Character/TitlePlates.h"

#include <algorithm>

#include "App/Platform/Windows/Winmain.h"
#include "Camera/CameraProjection.h"
#include "Camera/CameraState.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInventory.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Render/Text/CUIRenderText.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Character/TitlePlatesArt.h"

namespace UI::TitlePlates
{
namespace
{
using Art::Plate;

// The field (the coloured rectangle the text sits on) is the font's height and this much more. Every title and every
// rarity gets the same field, so every plate keeps one size whatever its text.
constexpr float FieldPadding = 5.f;
// The field's ends are points: at the text's top and bottom they are about this much of the text's height in.
constexpr float TipInset = 0.55f;
// The room the text keeps on each side, inside the points.
constexpr float TextMargin = 3.f;

// The text's colour on each rarity's plate (light tints of the plate's colours).
constexpr unsigned char TextColor[GameLogic::Titles::RarityCount][3] = {
    {244, 247, 250}, // silver: the plain silver plate
    {241, 227, 255}, // ice: the purple plate
    {220, 255, 226}, // jade: the green plate
    {226, 246, 255}, // gold: the blue plate with silver wings
    {255, 214, 200}, // crimson: the red plate with the crown
    {255, 236, 170}, // violet: the laurels
};

GLuint Texture()
{
    // Its own entry; loaded once, kept until the game ends.
    static GLuint texture = 0;
    static bool tried = false;
    if (!tried)
    {
        tried = true;
        texture = Bitmaps.LoadImageExclusive(L"Data\\Interface\\newui_titles.tga", GL_LINEAR, GL_CLAMP_TO_EDGE);
        if (texture == BITMAP_UNKNOWN)
            texture = 0;
    }
    return texture;
}

SIZE Measure(const std::wstring& text, HFONT font)
{
    g_pRenderText->SetFont(font);
    SIZE size = g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size()));
    size.cx += 2;
    return size;
}

// The party HP bar's top over a party member (as RenderPartyHP places it: 100 over the head), else bottomY.
int OverPartyBar(const CHARACTER& owner, int bottomY)
{
    if (PartyNumber <= 0 || g_Camera.TopViewEnable)
        return bottomY;

    for (int j = 0; j < PartyNumber; ++j)
    {
        const int index = Party[j].index;
        if (index <= -1 || &CharactersClient[index] != &owner)
            continue;

        const OBJECT* o = &owner.Object;
        vec3_t position;
        Vector(o->Position[0], o->Position[1], o->Position[2] + o->BoundingBoxMax[2] + 100.f, position);
        int x = 0;
        int y = 0;
        CameraProjection::WorldToScreen(g_Camera, position, &x, &y);
        // The bar's hover text ("HP : 50%") starts 6 over it; 2 more keep the plate's glow off it.
        return std::min(bottomY, y - 8);
    }
    return bottomY;
}

// The title in its rarity's colour over a 1 px dark shadow, so it reads on the plate's glints too.
void Text(const GameLogic::Titles::Worn& title, HFONT font, int x, int y, const SIZE& size)
{
    const auto* color = TextColor[static_cast<int>(title.Color)];
    g_pRenderText->SetFont(font);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(0, 0, 0, 255);
    g_pRenderText->RenderText(x + 1, y + 1, title.Text().c_str(), size.cx, size.cy, RT3_SORT_LEFT);
    g_pRenderText->SetTextColor(color[0], color[1], color[2], 255);
    g_pRenderText->RenderText(x, y, title.Text().c_str(), size.cx, size.cy, RT3_SORT_LEFT);
}
} // namespace

bool Enabled()
{
    return GameConfig::GetInstance().GetOverheadTitles();
}

int Render(const GameLogic::Titles::Worn& title, const CHARACTER& owner, int centerX, int bottomY)
{
    bottomY = OverPartyBar(owner, bottomY);
    const std::wstring& text = title.Text();
    const GLuint texture = Texture();
    if (text.empty() || texture == 0)
        return 0;

    // One scale for every plate (the atlas keeps the drawn plates' sizes), set by the font: the same field for all.
    HFONT font = g_hFontBold;
    SIZE size = Measure(text, font);
    const float k = (static_cast<float>(size.cy) + FieldPadding) / Art::FieldHeight;
    const float room = Art::FieldWidth * k - 2.f * (TipInset * static_cast<float>(size.cy) + TextMargin);
    if (static_cast<float>(size.cx) > room)
    {
        // A title too long for the field in bold gets the regular font; the plate keeps its size.
        font = g_hFont;
        size = Measure(text, font);
    }

    const Plate& plate = Art::Plates[static_cast<int>(title.Color)];
    const float height = plate.height * k;
    const float top = static_cast<float>(bottomY) - height;
    const float left = static_cast<float>(centerX) - (plate.fieldLeft + plate.fieldRight) / 2.f * k;
    RenderBitmap(static_cast<int>(texture), left, top, plate.width * k, height, plate.x / Art::AtlasSize,
                 plate.y / Art::AtlasSize, plate.width / Art::AtlasSize, plate.height / Art::AtlasSize, true, true,
                 0.f);

    const float fieldMiddle = top + (plate.fieldTop + plate.fieldBottom) / 2.f * k;
    const int textX = static_cast<int>(static_cast<float>(centerX) - static_cast<float>(size.cx) / 2.f + 0.5f);
    const int textY = static_cast<int>(fieldMiddle - static_cast<float>(size.cy) / 2.f + 0.5f);
    Text(title, font, textX, textY, size);

    g_pRenderText->SetFont(g_hFont);
    return static_cast<int>(height + 0.5f);
}
} // namespace UI::TitlePlates
