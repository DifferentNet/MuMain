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

namespace UI::TitlePlates
{
namespace
{
using GameLogic::Titles::Rarity;

// Where tools/make_title_plates.ps1 puts the pieces, in texture pixels.
constexpr float AtlasSize = 512.f;
constexpr float RibbonRow = 52.f;
constexpr float RibbonHeight = 48.f;
constexpr float RibbonLeft = 48.f;
constexpr float RibbonMiddle = 8.f;
constexpr float RibbonRight = 24.f;
// The ribbon's body inside its 48 rows (the rest is glow), the point of each end, where the text may start (after
// the gem) and the flat part of the right end.
constexpr float RibbonBodyTop = 5.f;
constexpr float RibbonBody = 38.f;
constexpr float RibbonTextStart = 41.f;
constexpr float RibbonRightFlat = 4.f;

constexpr float BannerX = 96.f;
constexpr float BannerRow = 84.f;
constexpr float WingWidth = 96.f;
constexpr float WingHeight = 80.f;
constexpr float FrameEnd = 32.f;
constexpr float FrameMiddle = 8.f;
constexpr float FrameHeight = 64.f;
constexpr float GemSize = 28.f;
// The frame's body inside its 64 rows, its core (where the text sits) and where the text may start.
constexpr float FrameBodyTop = 7.f;
constexpr float FrameBody = 50.f;
constexpr float FrameCore = 41.f;
constexpr float FrameTextStart = 27.f;
// The wings' root (where the feathers meet) in the left wing's piece; the right wing is its mirror.
constexpr float WingRootX = 85.7f;
constexpr float WingRootY = 49.f;
// The gem's diamond across, in its piece.
constexpr float GemAcross = 20.f;

// The text's colour on each rarity's plate (the concept's "hi" colours).
constexpr unsigned char TextColor[GameLogic::Titles::RarityCount][3] = {
    {244, 247, 250}, // silver
    {226, 246, 255}, // ice
    {220, 255, 226}, // jade
    {255, 236, 170}, // gold
    {255, 214, 200}, // crimson
    {241, 227, 255}, // violet
};

GLuint Texture()
{
    // Its own entry (not shared with RmlUi's copy of the same file); loaded once, kept until the game ends.
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

void Piece(GLuint texture, float x, float y, float width, float height, float sx, float sy, float sw, float sh)
{
    if (width <= 0.f || height <= 0.f)
        return;
    RenderBitmap(static_cast<int>(texture), x, y, width, height, sx / AtlasSize, sy / AtlasSize, sw / AtlasSize,
                 sh / AtlasSize, true, true, 0.f);
}

// A stretched middle piece samples only its centre column, so no neighbour bleeds in under the filter.
void Middle(GLuint texture, float x, float y, float width, float height, float sx, float sy, float sh)
{
    Piece(texture, x, y, width, height, sx + 2.f, sy, 4.f, sh);
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

void Text(const GameLogic::Titles::Worn& title, int x, int y, const SIZE& size)
{
    const auto* color = TextColor[static_cast<int>(title.Color)];
    g_pRenderText->SetFont(g_hFontBold);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(color[0], color[1], color[2], 255);
    g_pRenderText->RenderText(x, y, title.Text().c_str(), size.cx, size.cy, RT3_SORT_LEFT);
}

int RenderRibbon(GLuint texture, const GameLogic::Titles::Worn& title, const SIZE& text, int centerX, int bottomY)
{
    // The body is the text's height and a little; k turns texture pixels into screen ones.
    const float body = static_cast<float>(text.cy) + 3.f;
    const float k = body / RibbonBody;
    const float height = RibbonHeight * k;
    const float pad = 3.f * k;

    // left end (gem) | middle under the text | right end
    const float middle = std::max(0.f, static_cast<float>(text.cx) + pad - (RibbonLeft - RibbonTextStart) * k -
                                           RibbonRightFlat * k);
    const float width = (RibbonLeft + RibbonRight) * k + middle;
    const float x = static_cast<float>(centerX) - width / 2.f;
    const float y = static_cast<float>(bottomY) - height;
    const float row = static_cast<float>(static_cast<int>(title.Color)) * RibbonRow;

    Piece(texture, x, y, RibbonLeft * k, height, 0.f, row, RibbonLeft, RibbonHeight);
    Middle(texture, x + RibbonLeft * k, y, middle, height, RibbonLeft + 4.f, row, RibbonHeight);
    Piece(texture, x + RibbonLeft * k + middle, y, RibbonRight * k, height, RibbonLeft + RibbonMiddle + 8.f, row,
          RibbonRight, RibbonHeight);

    // The text between the gem and the right end, in the middle of the body.
    const float room = width - RibbonTextStart * k - (RibbonRight - RibbonRightFlat) * k;
    const int textX = static_cast<int>(x + RibbonTextStart * k + std::max(0.f, (room - text.cx) / 2.f) + 0.5f);
    const int textY = static_cast<int>(y + (RibbonBodyTop + RibbonBody / 2.f) * k - text.cy / 2.f + 0.5f);
    Text(title, textX, textY, text);
    return static_cast<int>(height + 0.5f);
}

int RenderBanner(GLuint texture, const GameLogic::Titles::Worn& title, const SIZE& text, int centerX, int bottomY)
{
    const float core = static_cast<float>(text.cy) + 6.f;
    const float k = core / FrameCore;
    const float height = FrameHeight * k;
    const float pad = 8.f * k;

    const float middle =
        std::max(0.f, static_cast<float>(text.cx) + 2.f * (FrameTextStart * k + pad) - 2.f * FrameEnd * k);
    const float width = 2.f * FrameEnd * k + middle;
    const float x = static_cast<float>(centerX) - width / 2.f;
    const float bodyHeight = FrameBody * k;

    // The wings are 1.3 times the frame's height and hang a little under it; the plate's bottom is theirs.
    const float wk = 1.3f * bodyHeight / (WingHeight - 2.f);
    const float wingHeight = WingHeight * wk;
    const float wingWidth = WingWidth * wk;
    const float centerY = static_cast<float>(bottomY) - (wingHeight - WingRootY * wk) - 0.15f * bodyHeight + 2.f * wk;
    const float y = centerY - (FrameBodyTop + FrameBody / 2.f) * k;
    const float rootY = centerY + 0.15f * bodyHeight;
    const float rootInside = 0.2f * bodyHeight;

    const float row = static_cast<float>(static_cast<int>(title.Color)) * BannerRow;
    const float leftRootX = x + 2.f * k + rootInside;
    const float rightRootX = x + width - 2.f * k - rootInside;
    Piece(texture, leftRootX - WingRootX * wk, rootY - WingRootY * wk, wingWidth, wingHeight, BannerX, row, WingWidth,
          WingHeight);
    Piece(texture, rightRootX - (WingWidth - WingRootX) * wk, rootY - WingRootY * wk, wingWidth, wingHeight,
          BannerX + WingWidth + 4.f, row, WingWidth, WingHeight);

    const float frameX = BannerX + 2.f * (WingWidth + 4.f);
    Piece(texture, x, y, FrameEnd * k, height, frameX, row, FrameEnd, FrameHeight);
    Middle(texture, x + FrameEnd * k, y, middle, height, frameX + FrameEnd + 4.f, row, FrameHeight);
    Piece(texture, x + FrameEnd * k + middle, y, FrameEnd * k, height, frameX + FrameEnd + FrameMiddle + 8.f, row,
          FrameEnd, FrameHeight);

    // The gem sits on the frame's top edge, as wide as half the frame is high.
    const float gk = 0.55f * bodyHeight / GemAcross;
    const float gem = GemSize * gk;
    const float bodyTop = y + FrameBodyTop * k;
    Piece(texture, static_cast<float>(centerX) - gem / 2.f, bodyTop - gem / 2.f, gem, gem,
          frameX + 2.f * FrameEnd + FrameMiddle + 12.f, row, GemSize, GemSize);

    const int textX = static_cast<int>(static_cast<float>(centerX) - text.cx / 2.f + 0.5f);
    const int textY = static_cast<int>(centerY - text.cy / 2.f + 0.5f);
    Text(title, textX, textY, text);

    const float top = std::min(rootY - WingRootY * wk, bodyTop - gem / 2.f);
    return static_cast<int>(static_cast<float>(bottomY) - top + 0.5f);
}
} // namespace

bool Enabled()
{
    return GameConfig::GetInstance().GetOverheadTitles();
}

bool IsBanner(Rarity rarity)
{
    return rarity == Rarity::Gold || rarity == Rarity::Crimson;
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

    const int height = IsBanner(title.Color) ? RenderBanner(texture, title, size, centerX, bottomY)
                                             : RenderRibbon(texture, title, size, centerX, bottomY);
    g_pRenderText->SetFont(g_hFont);
    return height;
}
} // namespace UI::TitlePlates
