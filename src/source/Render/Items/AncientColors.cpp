#include "stdafx.h"
#include "Render/Items/AncientColors.h"

#include "Core/Text/Utf8.h"
#include "Data/GameData/ItemData/ItemModelSlots.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "GameLogic/Items/AncientLook.h"
#include "GameLogic/Items/CSItemOption.h"
#include "Render/Items/AncientColorRules.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Sprites/GlobalBitmap.h"

#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::Items::AncientColors
{
namespace
{
// The ancient discriminator is the low bits of the item's value; the rest is other options.
constexpr int DiscriminatorMask = 0x03;
// The cache key: the texture's number and the set.
constexpr int SetBits = 8;

// The set of the body part being drawn (ScopedPartSet).
int g_partSet = 0;
// The colour of the model being recoloured (ScopedSetTextures), for the pulse.
const SetColor* g_drawnColor = nullptr;

// Recoloured copies by texture and set; BITMAP_UNKNOWN for a texture that keeps its look (skin, hair, a failure).
std::unordered_map<std::uint64_t, GLuint> g_copies;

std::uint64_t KeyOf(GLuint texture, int set)
{
    return (static_cast<std::uint64_t>(texture) << SetBits) | static_cast<std::uint64_t>(set);
}

// The own character's piece from its equipment: the real discriminator, not only "ancient".
int OwnDiscriminator(int bodyPart, int itemType)
{
    if (CharacterMachine == nullptr)
        return 0;
    const ITEM& item = CharacterMachine->Equipment[EQUIPMENT_HELM + bodyPart - BODYPART_HELM];
    return item.Type == itemType ? item.AncientDiscriminator & DiscriminatorMask : 0;
}

bool KeepsLook(const BMD& model, int mesh, const BITMAP_t& texture)
{
    if (texture.IsSkin || texture.IsHair)
        return true;
    const char* fileName = model.Textures[mesh].FileName;
    const std::wstring meshTexture =
        Core::Text::FromUtf8(std::string(fileName, strnlen(fileName, sizeof(model.Textures[mesh].FileName))));
    return KeepsTexture(meshTexture) || KeepsTexture(texture.FileName);
}

GLuint MakeCopy(const BITMAP_t& texture, const SetColor& color)
{
    const std::wstring name = std::wstring(texture.FileName) + L"|ancient" + std::to_wstring(color.set);
    const CGlobalBitmap::PixelChange change = [&color](std::uint8_t* pixels, std::size_t count, int components) {
        Recolor(pixels, count, components, color);
    };
    return Bitmaps.LoadChangedImage(texture.FileName, name, GL_NEAREST,
                                    texture.wrapRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE, change);
}

// The recoloured copy of a mesh's texture, made the first time; BITMAP_UNKNOWN when it keeps its look.
GLuint CopyOf(const BMD& model, int mesh, const SetColor& color)
{
    const GLuint texture = model.IndexTexture[mesh];
    const std::uint64_t key = KeyOf(texture, color.set);
    if (const auto known = g_copies.find(key); known != g_copies.end())
        return known->second;

    const BITMAP_t* bitmap = Bitmaps.FindTexture(texture);
    const GLuint copy = bitmap == nullptr || KeepsLook(model, mesh, *bitmap) ? BITMAP_UNKNOWN : MakeCopy(*bitmap, color);
    g_copies.emplace(key, copy);
    return copy;
}

// The recoloured copy of a texture that belongs to no mesh (a cloth's); BITMAP_UNKNOWN when it keeps its look.
GLuint CopyOfTexture(GLuint texture, const SetColor& color)
{
    const std::uint64_t key = KeyOf(texture, color.set);
    if (const auto known = g_copies.find(key); known != g_copies.end())
        return known->second;

    const BITMAP_t* bitmap = Bitmaps.FindTexture(texture);
    // A copy made earlier (a cloth created while its model was recoloured) is not copied again.
    const bool keeps = bitmap == nullptr || bitmap->IsSkin || bitmap->IsHair || KeepsTexture(bitmap->FileName) ||
                       std::wstring_view(bitmap->FileName).find(L"|ancient") != std::wstring_view::npos;
    const GLuint copy = keeps ? BITMAP_UNKNOWN : MakeCopy(*bitmap, color);
    g_copies.emplace(key, copy);
    return copy;
}
} // namespace

int SetOfBodyPart(const CHARACTER* c, int bodyPart)
{
    if (c == nullptr || bodyPart < BODYPART_HELM || bodyPart > BODYPART_BOOTS)
        return 0;
    const PART_t& part = c->BodyPart[bodyPart];
    if (!Data::Items::IsItemModelSlot(part.Type))
        return 0;

    const int itemType = Data::Items::ToItemType(part.Type);
    if (part.AncientDiscriminator == 0)
    {
        // A plain piece may take the colour of the set the armour shows (BorrowedSet).
        return bodyPart == BODYPART_ARMOR ? 0 : BorrowedSet(itemType, SetOfBodyPart(c, BODYPART_ARMOR));
    }
    int discriminator = c == Hero ? OwnDiscriminator(bodyPart, itemType) : 0;
    if (discriminator == 0)
        discriminator = GameLogic::AncientLook::DiscriminatorOf(std::wstring_view(c->ID), bodyPart - BODYPART_HELM);
    return discriminator == 0 ? 0 : PickSet(g_csItemOption.GetSetsOfItem(itemType), discriminator);
}

ScopedPartSet::ScopedPartSet(int set)
    : m_previous(g_partSet)
{
    g_partSet = set;
}

ScopedPartSet::~ScopedPartSet()
{
    g_partSet = m_previous;
}

ScopedSetTextures::ScopedSetTextures(BMD* model)
    : m_previousColor(g_drawnColor), m_partSet(g_partSet)
{
    // Only the part's own model: another model drawn while it is drawn keeps its look.
    g_partSet = 0;
    const SetColor* color = FindSetColor(m_partSet);
    if (color == nullptr || model == nullptr || model->IndexTexture == nullptr || model->Textures == nullptr)
        return;

    m_model = model;
    for (int mesh = 0; mesh < model->NumMeshs; ++mesh)
    {
        const GLuint copy = CopyOf(*model, mesh, *color);
        if (copy == BITMAP_UNKNOWN)
            continue;
        m_swapped.emplace_back(mesh, model->IndexTexture[mesh]);
        model->IndexTexture[mesh] = copy;
    }
    g_drawnColor = color;
}

ScopedSetTextures::~ScopedSetTextures()
{
    for (const auto& [mesh, texture] : m_swapped)
        m_model->IndexTexture[mesh] = texture;
    g_drawnColor = static_cast<const SetColor*>(m_previousColor);
    g_partSet = m_partSet;
}

std::uint32_t PartTexture(std::uint32_t texture)
{
    if (g_drawnColor == nullptr)
        return texture;
    const GLuint copy = CopyOfTexture(texture, *g_drawnColor);
    return copy == BITMAP_UNKNOWN ? texture : copy;
}

bool PulseColor(float color[3])
{
    if (g_drawnColor == nullptr)
        return false;
    color[0] = g_drawnColor->r;
    color[1] = g_drawnColor->g;
    color[2] = g_drawnColor->b;
    return true;
}
} // namespace Render::Items::AncientColors
