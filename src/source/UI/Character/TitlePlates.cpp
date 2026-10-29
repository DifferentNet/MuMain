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

// The plate's core (its coloured field) is the text's height and this much more.
constexpr float CorePadding = 5.f;
// The room the text keeps on each side, inside where the core starts to narrow.
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

// Where and how big a plate is drawn: k screen units per atlas pixel, the straight part on each side in atlas
// pixels (0: the ends meet the middle).
struct Layout
{
    float k;
    float straight;
};

Layout Fit(const Plate& plate, const SIZE& text)
{
    const float k = (static_cast<float>(text.cy) + CorePadding) / (plate.coreBottom - plate.coreTop);
    const float straightLeft = plate.decoLeft - plate.capLeft;
    const float straightRight = plate.capRight - plate.decoRight;
    // The text's room when the straight parts are cut down to nothing; each atlas pixel of them adds one.
    const float shortestRoom = plate.textRight - plate.textLeft - straightLeft - straightRight;
    const float needed = (static_cast<float>(text.cx) + 2.f * TextMargin) / k;
    return Layout{k, std::max(0.f, (needed - shortestRoom) / 2.f)};
}

// Draws the plate's atlas columns [from, to) at x; returns where the next piece starts.
float Columns(GLuint texture, const Plate& plate, float from, float to, float x, float y, float k)
{
    const float width = (to - from) * k;
    if (width <= 0.f)
        return x;
    RenderBitmap(static_cast<int>(texture), x, y, width, plate.height * k, (plate.x + from) / Art::AtlasSize,
                 plate.y / Art::AtlasSize, (to - from) / Art::AtlasSize, plate.height / Art::AtlasSize, true, true,
                 0.f);
    return x + width;
}

// Draws one atlas column (at its centre) stretched to width at x; returns where the next piece starts.
float Stretched(GLuint texture, const Plate& plate, float column, float width, float x, float y, float k)
{
    if (width <= 0.f)
        return x;
    RenderBitmap(static_cast<int>(texture), x, y, width, plate.height * k, (plate.x + column) / Art::AtlasSize,
                 plate.y / Art::AtlasSize, 0.f, plate.height / Art::AtlasSize, true, true, 0.f);
    return x + width;
}

// The left end, the straight part (its own pixels as far as they go, then its cleanest column stretched), the
// middle, and the same mirrored on the right; the middle is centred on centerX.
void Frame(GLuint texture, const Plate& plate, const Layout& layout, float centerX, float y)
{
    const float k = layout.k;
    const float ownLeft = std::min(layout.straight, plate.decoLeft - plate.capLeft);
    const float ownRight = std::min(layout.straight, plate.capRight - plate.decoRight);

    float x = centerX - (plate.capLeft + layout.straight + (plate.decoRight - plate.decoLeft) / 2.f) * k;
    x = Columns(texture, plate, 0.f, plate.capLeft + ownLeft, x, y, k);
    x = Stretched(texture, plate, plate.cleanLeft, (layout.straight - ownLeft) * k, x, y, k);
    x = Columns(texture, plate, plate.decoLeft, plate.decoRight, x, y, k);
    x = Stretched(texture, plate, plate.cleanRight, (layout.straight - ownRight) * k, x, y, k);
    Columns(texture, plate, plate.capRight - ownRight, plate.width, x, y, k);
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
void Text(const GameLogic::Titles::Worn& title, int x, int y, const SIZE& size)
{
    const auto* color = TextColor[static_cast<int>(title.Color)];
    g_pRenderText->SetFont(g_hFontBold);
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

    g_pRenderText->SetFont(g_hFontBold);
    SIZE size = g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size()));
    size.cx += 2;

    const Plate& plate = Art::Plates[static_cast<int>(title.Color)];
    const Layout layout = Fit(plate, size);
    const float height = plate.height * layout.k;
    const float top = static_cast<float>(bottomY) - height;
    Frame(texture, plate, layout, static_cast<float>(centerX), top);

    const float coreMiddle = top + (plate.coreTop + plate.coreBottom) / 2.f * layout.k;
    const int textX = static_cast<int>(static_cast<float>(centerX) - static_cast<float>(size.cx) / 2.f + 0.5f);
    const int textY = static_cast<int>(coreMiddle - static_cast<float>(size.cy) / 2.f + 0.5f);
    Text(title, textX, textY, size);

    g_pRenderText->SetFont(g_hFont);
    return static_cast<int>(height + 0.5f);
}
} // namespace UI::TitlePlates
