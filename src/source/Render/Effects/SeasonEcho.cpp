#include "stdafx.h"
#include "Render/Effects/SeasonEcho.h"

#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/SeasonEchoStore.h"
#include "GameLogic/Events/SeasonMarks.h"
#include "Render/Effects/EchoTrail.h"
#include "Render/Items/ItemDisplay.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Mobile/InterfaceMode.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

extern float BoneScale;

namespace Render::Effects::SeasonEcho
{
namespace
{
namespace Trail = Render::Effects::EchoTrail;

// Other players' echoes: the nearest this many within OthersRange of the hero.
constexpr int MaxOthersPc = 8;
constexpr int MaxOthersPhone = 3;
constexpr float OthersRange = 1200.f; // 12 tiles
// The own echo and the others'.
constexpr int MaxEntries = 1 + MaxOthersPc;
// Weapons, wings, a quiver: the linked items a copy keeps.
constexpr int MaxLinked = 5;
// Other players' echoes and the additive layer need this render level (Options, Graphics).
constexpr int MinRenderLevel = 2;
// A silhouette's alpha (an opaque plain mesh drawn flat in the season's colour) as a share of the copy's.
constexpr float SilhouetteShare = 0.85f;
// RENDER_COLOR draws opaque, unblended, from alpha 0.99 on.
static_assert(Trail::StartAlpha * SilhouetteShare < 0.99f, "a silhouette must stay translucent");
// The meshes drawn in light are added in the season's colour times the copy's alpha times this, at most the full
// colour: their textures are mostly dark (a wing's feathers on black), so a copy keeps them at full light for most
// of its life and only fades them at the end.
constexpr float GlowGain = 3.f;
// The additive layer's share of the season's colour on the opaque plain meshes, times the copy's alpha: their
// texture's detail over the silhouette (PC only).
constexpr float BrightShare = 0.75f;
// RenderMesh's blend mesh index that sends any mesh down its blend path: added in light (ONE, ONE), coloured
// BodyLight times the blend light.
constexpr int AnyMeshBlends = -2;
// A copy fainter than this isn't drawn.
constexpr float MinAlpha = 0.004f;
constexpr std::size_t NameCapacity = MAX_MONSTER_NAME + 1;

// One model's pose as it was drawn: its bones and what Transform() was given with them.
struct Pose
{
    vec3_t origin{};
    float bodyScale = 1.f;
    float skinScale = 0.f;
    float boneScale = 1.f;
    bool translate = true;
    float bones[MAX_BONES][3][4];
};

// How the live model draws its meshes, which a copy follows: RenderBody's BlendMesh and HiddenMesh.
struct MeshLook
{
    // The meshes whose texture has this index are drawn in light; every mesh at -2 or less, none at -1.
    int blendMesh = -1;
    // This mesh isn't drawn.
    int hiddenMesh = -1;
    // Nor this one: the cloth's mesh of some pants (the copy has no cloth).
    int clothMesh = -1;
};

// A model of a copy and how its meshes are drawn.
struct Drawn
{
    int model = -1;
    MeshLook look;
};

// One copy: the character's body parts (one skeleton), its linked items and its mount, as they were drawn on the
// frame it was taken.
struct Snapshot
{
    // At least one body part was taken: only then it is drawn.
    bool valid = false;
    // Where the character stood.
    float x = 0.f;
    float y = 0.f;

    bool hasBody = false;
    Pose body;
    Drawn parts[MAX_BODYPART];
    int partCount = 0;

    Pose linked[MaxLinked];
    Drawn linkedItems[MaxLinked];
    int linkedCount = 0;

    bool hasMount = false;
    Pose mount;
    Drawn mountDrawn;

    void Start(float atX, float atY)
    {
        valid = false;
        x = atX;
        y = atY;
        hasBody = false;
        partCount = 0;
        linkedCount = 0;
        hasMount = false;
        mountDrawn = {};
    }
};

// A character whose echo shows: its trail and the poses of its copies by slot.
struct Entry
{
    bool active = false;
    OBJECT* object = nullptr;
    // The character's name when it was chosen: an object slot taken over by another character doesn't keep its echo.
    wchar_t name[NameCapacity]{};
    bool own = false;
    int season = 0;
    float color[3] = {0.62f, 0.36f, 1.f};
    Trail::Trail trail;
    // The frame (WorldTime) its trail last moved: a character drawn twice in a frame takes one step.
    double steppedAt = -1.0;
    // The copy taken this frame, which its mount (drawn after the characters) joins.
    int pendingSlot = -1;
    double pendingAt = -1.0;
    Snapshot snaps[Trail::MaxCopies];
};

// Allocated once, when the first echo shows (about 2 MB: each pose keeps a full bone palette, which the GPU skinning
// reads whole).
struct Pool
{
    Entry entries[MaxEntries];
};

std::unique_ptr<Pool> s_pool;
int s_activeCount = 0;

// The character whose models are copied now, and the copy they go to.
struct Capture
{
    const OBJECT* object = nullptr;
    Snapshot* snap = nullptr;
    // The linked item taken last, whose look NoteLinkedItemDrawn brings once it was drawn; -1 for none.
    int lastLinked = -1;
};
Capture s_capture;

bool IsPhone()
{
    return UI::Mobile::IsPhoneView() || UI::Mobile::IsTouchControls();
}

Trail::Settings SettingsNow()
{
    return IsPhone() ? Trail::PhoneSettings() : Trail::PcSettings();
}

bool RenderLevelAtLeast(int level)
{
    auto* const option = g_pOption;
    return option != nullptr && option->GetRenderLevel() >= level;
}

void CopyBones(const float (*from)[3][4], float (&to)[MAX_BONES][3][4])
{
    std::memcpy(to, from, sizeof(to));
}

bool IsModel(int model)
{
    return model >= 0 && model < MAX_MODELS;
}

// The mounts a character rides. RenderMount draws the pets too (the Guardian Angel, the Imp, the Dark Raven), which
// a copy never takes.
bool IsRidingMount(int model)
{
    switch (model)
    {
    case MODEL_UNICON:
    case MODEL_PEGASUS:
    case MODEL_DARK_HORSE:
    case MODEL_FENRIR_BLACK:
    case MODEL_FENRIR_BLUE:
    case MODEL_FENRIR_RED:
    case MODEL_FENRIR_GOLD:
        return true;
    default:
        return false;
    }
}

// The cloth of these pants is drawn on one of their meshes; the echo has no cloth, so that mesh stays out.
int ClothMeshOf(int model)
{
    switch (model)
    {
    case MODEL_GRAND_SOUL_PANTS:
        return 2;
    case MODEL_DIVINE_PANTS:
        return 3;
    case MODEL_DARK_SOUL_PANTS:
        return 2;
    default:
        return -1;
    }
}

Entry* FindActive(const OBJECT* object)
{
    if (s_pool == nullptr || s_activeCount == 0 || object == nullptr)
        return nullptr;
    for (Entry& entry : s_pool->entries)
    {
        if (entry.active && entry.object == object)
            return &entry;
    }
    return nullptr;
}

// Clears the trail and its copies; the entry stays.
void Forget(Entry& entry)
{
    entry.trail.Clear();
    for (Snapshot& snap : entry.snaps)
        snap.valid = false;
    entry.pendingSlot = -1;
    entry.pendingAt = -1.0;
}

void Drop(Entry& entry)
{
    if (!entry.active)
        return;
    Forget(entry);
    entry.active = false;
    entry.object = nullptr;
    entry.name[0] = L'\0';
    entry.steppedAt = -1.0;
    --s_activeCount;
}

void DropAll()
{
    if (s_pool == nullptr)
        return;
    for (Entry& entry : s_pool->entries)
        Drop(entry);
}

// Whether the character may leave copies now.
bool LeavesCopies(const CHARACTER* c, OBJECT* o)
{
    return o->Live && o->Kind == KIND_PLAYER && o->Type == MODEL_PLAYER && c->Dead <= 0 &&
           !g_isCharacterBuff(o, eBuff_Cloaking) && !gMapManager.InChaosCastle();
}

// A model's state Transform() and drawing change, put back when the copy is drawn: the models are shared by every
// character, and other code reads what the last real transform left.
class KeptModelState
{
public:
    explicit KeptModelState(BMD& model)
        : m_model(model), m_bodyScale(model.BodyScale), m_lightEnable(model.LightEnable), m_hideSkin(model.HideSkin),
          m_contrastEnable(model.ContrastEnable), m_streamMesh(model.StreamMesh),
          m_bones(model.m_pCurrentBoneTransform), m_translate(model.m_LastTranslate),
          m_skinScale(model.m_LastSkinScale), m_boneScale(model.m_LastBoneScale)
    {
        VectorCopy(model.BodyLight, m_bodyLight);
        VectorCopy(model.BodyOrigin, m_bodyOrigin);
    }

    ~KeptModelState()
    {
        VectorCopy(m_bodyLight, m_model.BodyLight);
        VectorCopy(m_bodyOrigin, m_model.BodyOrigin);
        m_model.BodyScale = m_bodyScale;
        m_model.LightEnable = m_lightEnable;
        m_model.HideSkin = m_hideSkin;
        m_model.ContrastEnable = m_contrastEnable;
        m_model.StreamMesh = m_streamMesh;
        m_model.m_pCurrentBoneTransform = m_bones;
        m_model.m_LastTranslate = m_translate;
        m_model.m_LastSkinScale = m_skinScale;
        m_model.m_LastBoneScale = m_boneScale;
        // The shared skinned-vertex scratch may hold the copy's vertices now: the next reader skins them again
        // from the bones put back.
        m_model.m_SkinStamp = 0;
    }

    KeptModelState(const KeptModelState&) = delete;
    KeptModelState& operator=(const KeptModelState&) = delete;

private:
    BMD& m_model;
    vec3_t m_bodyLight{};
    vec3_t m_bodyOrigin{};
    float m_bodyScale;
    bool m_lightEnable;
    bool m_hideSkin;
    bool m_contrastEnable;
    char m_streamMesh;
    float (*m_bones)[3][4];
    bool m_translate;
    float m_skinScale;
    float m_boneScale;
};

enum class MeshKind
{
    None,  // not drawn: hidden, empty or a hidden texture
    Plain, // drawn faded in the season's colour: a flat silhouette, or its texture tinted when it is cut out
    Glow,  // drawn in light by the live model: added in the season's colour
};

// What a copy does with a model's mesh, as RenderBody draws it: a texture script's bright mesh takes its own index as
// the blend mesh, and a mesh is drawn in light when its texture is the blend mesh (every mesh at -2 or less). The
// shadow passes of the texture scripts stay out.
MeshKind KindOf(BMD& model, int i, const MeshLook& look)
{
    if (i == look.hiddenMesh || i == look.clothMesh)
        return MeshKind::None;
    Mesh_t& mesh = model.Meshs[i];
    if (mesh.NumTriangles == 0 || model.IndexTexture[mesh.Texture] == BITMAP_HIDE)
        return MeshKind::None;
    int blendMesh = look.blendMesh;
    if (mesh.m_csTScript != nullptr)
    {
        if (mesh.m_csTScript->getHiddenMesh())
            return MeshKind::None;
        if (mesh.m_csTScript->getBright())
            blendMesh = i;
    }
    return blendMesh <= -2 || mesh.Texture == blendMesh ? MeshKind::Glow : MeshKind::Plain;
}

void SetBodyLight(BMD& model, const float (&color)[3], float scale)
{
    model.BodyLight[0] = color[0] * scale;
    model.BodyLight[1] = color[1] * scale;
    model.BodyLight[2] = color[2] * scale;
}

// A cut-out mesh: its texture has an alpha channel (four components). GetTexture never gives null: a missing
// texture is an empty error bitmap.
bool IsCutOut(BMD& model, int i)
{
    return Bitmaps.GetTexture(model.IndexTexture[model.Meshs[i].Texture])->Components == 4;
}

// The faded layer, on every device. An opaque plain mesh is a flat silhouette in the season's colour (its texture
// times the colour would keep dark gear dark: the colour can only darken it); a cut-out one (a four-component
// texture) is its texture times the colour, since a flat colour would fill its clear texels; both blended at the
// copy's alpha. The meshes the live model draws in light (a wing's bright texture) are added in the season's colour
// (ONE, ONE), their black adding nothing.
void DrawFaded(BMD& model, const MeshLook& look, float alpha, const float (&color)[3])
{
    const float glowLight = std::min(1.f, alpha * GlowGain);
    model.BeginRender(alpha);
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const MeshKind kind = KindOf(model, i, look);
        if (kind == MeshKind::Glow)
        {
            SetBodyLight(model, color, glowLight);
            model.RenderMesh(i, RENDER_TEXTURE, alpha, AnyMeshBlends, 1.f);
        }
        else if (kind == MeshKind::Plain)
        {
            SetBodyLight(model, color, 1.f);
            if (IsCutOut(model, i))
                model.RenderMesh(i, RENDER_TEXTURE, alpha);
            else
                model.RenderMesh(i, RENDER_COLOR, alpha * SilhouetteShare);
        }
    }
    model.EndRender();
}

// The additive layer (PC, render level 2 and up), added in the season's colour times the copy's alpha (ONE, ONE): the
// opaque plain meshes' textures once more, at BrightShare, so the gear's detail shows over its silhouette (cut-out,
// four-component textures left out: their clear texels would add their colour), and the meshes the live model draws
// in light, as it adds them, over what the faded layer added.
void DrawLight(BMD& model, const MeshLook& look, float alpha, const float (&color)[3])
{
    model.BeginRender(alpha);
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        const MeshKind kind = KindOf(model, i, look);
        if (kind == MeshKind::Glow)
        {
            SetBodyLight(model, color, alpha);
            model.RenderMesh(i, RENDER_TEXTURE, alpha, AnyMeshBlends, 1.f);
        }
        else if (kind == MeshKind::Plain && !IsCutOut(model, i))
        {
            SetBodyLight(model, color, alpha * BrightShare);
            model.RenderMesh(i, RENDER_TEXTURE | RENDER_BRIGHT, alpha);
        }
    }
    model.EndRender();
}

void DrawPose(const Drawn& drawn, Pose& pose, float alpha, const float (&color)[3], bool additive)
{
    if (!IsModel(drawn.model))
        return;
    BMD& model = Models[drawn.model];
    if (model.NumMeshs <= 0 || model.Meshs == nullptr)
        return;

    const KeptModelState kept(model);
    model.LightEnable = false; // flat colour, no new light on the hero's look
    model.HideSkin = false;
    model.ContrastEnable = false;
    model.StreamMesh = -1;
    model.BodyScale = pose.bodyScale;
    VectorCopy(pose.origin, model.BodyOrigin);
    BoneScale = pose.boneScale;

    // The copy's own bounds don't matter: nothing picks or culls it.
    OBB_t obb{};
    vec3_t noBounds = {0.f, 0.f, 0.f};
    model.Transform(pose.bones, noBounds, noBounds, &obb, pose.translate, pose.skinScale);

    DrawFaded(model, drawn.look, alpha, color);
    if (additive)
        DrawLight(model, drawn.look, alpha, color);
}

void DrawSnapshot(Snapshot& snap, float alpha, const float (&color)[3], bool additive)
{
    if (snap.hasBody)
    {
        for (int i = 0; i < snap.partCount; ++i)
            DrawPose(snap.parts[i], snap.body, alpha, color, additive);
    }
    for (int i = 0; i < snap.linkedCount; ++i)
        DrawPose(snap.linkedItems[i], snap.linked[i], alpha, color, additive);
    if (snap.hasMount)
        DrawPose(snap.mountDrawn, snap.mount, alpha, color, additive);
}

void DrawCopies()
{
    if (s_pool == nullptr || s_activeCount == 0)
        return;
    if (gMapManager.InChaosCastle())
    {
        for (Entry& entry : s_pool->entries)
        {
            if (entry.active)
                Forget(entry);
        }
        return;
    }

    const Trail::Settings settings = SettingsNow();
    const bool additive = !IsPhone() && RenderLevelAtLeast(MinRenderLevel);

    // Opened at the first copy drawn: a frame without one changes no state. Drawn after the opaque world, outside
    // the sun shadow's scope (neither cast nor received) and before the scene's bloom; the copies are tested against
    // the world's depth but write none, so they don't hide what is drawn after them, and their silhouettes stay flat
    // (ScopedFlatModelColor). The depth mask comes back last.
    std::optional<ScopedNoModelDepthWrite> noDepthWrite;
    std::optional<ScopedFlatModelColor> flatColor;
    std::optional<mu::ScopedBloomExclusion> noBloom;
    const auto activeBones = g_pActiveBoneTransform;
    const float boneScale = BoneScale;
    bool drawing = false;

    for (Entry& entry : s_pool->entries)
    {
        if (!entry.active || entry.object == nullptr)
            continue;
        for (int slot = 0; slot < Trail::MaxCopies; ++slot)
        {
            Snapshot& snap = entry.snaps[slot];
            if (!snap.valid || !entry.trail.IsAlive(slot, WorldTime, settings))
                continue;
            const float dx = entry.object->Position[0] - snap.x;
            const float dy = entry.object->Position[1] - snap.y;
            const float alpha =
                entry.trail.AlphaOf(slot, WorldTime, settings) * Trail::ClearanceFactor(std::sqrt(dx * dx + dy * dy));
            if (alpha < MinAlpha)
                continue;

            if (!drawing)
            {
                noDepthWrite.emplace();
                flatColor.emplace();
                noBloom.emplace();
                // A fading copy's alpha drops under the usual 0.25 cut: only clear texels are cut.
                mu::GetRenderer().SetAlphaFunc(GL_GREATER, 0.f);
                drawing = true;
            }
            DrawSnapshot(snap, alpha, entry.color, additive);
        }
    }

    if (drawing)
    {
        SetActiveBoneTransform(activeBones);
        BoneScale = boneScale;
        mu::GetRenderer().SetAlphaFunc(GL_GREATER, 0.25f);
        DisableAlphaBlend();
    }
}

struct Want
{
    OBJECT* object = nullptr;
    const wchar_t* name = nullptr;
    int season = 0;
    bool own = false;
    float distance = 0.f; // squared, from the hero
    bool matched = false;
};

bool SameName(const wchar_t* a, const wchar_t* b)
{
    return std::wstring_view(a) == std::wstring_view(b);
}

void Start(Entry& entry, const Want& want)
{
    entry.active = true;
    entry.object = want.object;
    const std::wstring_view name(want.name);
    const std::size_t length = std::min(name.size(), NameCapacity - 1);
    std::copy_n(name.data(), length, entry.name);
    entry.name[length] = L'\0';
    entry.own = want.own;
    entry.season = want.season;
    GameLogic::SeasonMarks::Marks().EchoColorOf(entry.season, entry.color);
    entry.steppedAt = -1.0;
    Forget(entry);
    ++s_activeCount;
}

// Who shows an echo next frame: the own character, then the nearest others.
int ChooseWanted(Want (&wants)[MaxEntries])
{
    int count = 0;
    if (Hero == nullptr || !Hero->Object.Live)
        return count;

    const GameLogic::SeasonEcho::Store& echoes = GameLogic::SeasonEcho::Echoes();
    if (const int season = echoes.OwnShownSeason(); season > 0)
        wants[count++] = {&Hero->Object, Hero->ID, season, true, 0.f, false};

    if (echoes.OthersShownCount() == 0 || !OtherEchoesOn() || !RenderLevelAtLeast(MinRenderLevel))
        return count;

    const int maxOthers = IsPhone() ? MaxOthersPhone : MaxOthersPc;
    const float heroX = Hero->Object.Position[0];
    const float heroY = Hero->Object.Position[1];
    // The nearest first.
    Want others[MaxOthersPc];
    int otherCount = 0;
    const int end = CharacterSlotEnd();
    for (int i = 0; i < end; ++i)
    {
        CHARACTER* c = &CharactersClient[i];
        if (c == Hero)
            continue;
        OBJECT* o = &c->Object;
        if (!o->Live || o->Kind != KIND_PLAYER || o->Type != MODEL_PLAYER)
            continue;
        const float dx = o->Position[0] - heroX;
        const float dy = o->Position[1] - heroY;
        const float distance = dx * dx + dy * dy;
        if (distance > OthersRange * OthersRange)
            continue;
        if (otherCount == maxOthers && distance >= others[otherCount - 1].distance)
            continue;
        const int season = echoes.ShownSeasonOf(c->ID);
        if (season <= 0)
            continue;

        int at = otherCount < maxOthers ? otherCount++ : maxOthers - 1;
        while (at > 0 && others[at - 1].distance > distance)
        {
            others[at] = others[at - 1];
            --at;
        }
        others[at] = {o, c->ID, season, false, distance, false};
    }
    for (int i = 0; i < otherCount; ++i)
        wants[count++] = others[i];
    return count;
}

void Choose()
{
    Want wants[MaxEntries];
    const int count = ChooseWanted(wants);
    if (count == 0 && s_activeCount == 0)
        return;
    if (s_pool == nullptr)
        s_pool = std::make_unique<Pool>();

    // Keep the entries still wanted, drop the others.
    for (Entry& entry : s_pool->entries)
    {
        if (!entry.active)
            continue;
        Want* found = nullptr;
        for (int i = 0; i < count; ++i)
        {
            Want& want = wants[i];
            if (!want.matched && want.object == entry.object && want.own == entry.own &&
                SameName(want.name, entry.name))
            {
                found = &want;
                break;
            }
        }
        if (found == nullptr)
        {
            Drop(entry);
            continue;
        }
        found->matched = true;
        if (entry.season != found->season)
        {
            entry.season = found->season;
            GameLogic::SeasonMarks::Marks().EchoColorOf(entry.season, entry.color);
        }
    }

    // Start the new ones.
    for (int i = 0; i < count; ++i)
    {
        if (wants[i].matched)
            continue;
        for (Entry& entry : s_pool->entries)
        {
            if (!entry.active)
            {
                Start(entry, wants[i]);
                break;
            }
        }
    }
}
} // namespace

void BeginCharacter(CHARACTER* c, OBJECT* o)
{
    s_capture = {};
    if (c == nullptr || SceneFlag != MAIN_SCENE)
        return;
    Entry* const entry = FindActive(o);
    if (entry == nullptr || entry->steppedAt == WorldTime)
        return;
    entry->steppedAt = WorldTime;

    if (!SameName(c->ID, entry->name) || !LeavesCopies(c, o))
    {
        Forget(*entry);
        return;
    }

    const int slot = entry->trail.Advance(o->Position[0], o->Position[1], WorldTime, SettingsNow());
    if (slot < 0)
        return;
    Snapshot& snap = entry->snaps[slot];
    snap.Start(o->Position[0], o->Position[1]);
    entry->pendingSlot = slot;
    entry->pendingAt = WorldTime;
    s_capture = {o, &snap};
}

void NoteBodyPart(const OBJECT* o, int model, int level, bool translate)
{
    if (s_capture.object == nullptr || s_capture.object != o || o->EnableShadow || o->BoneTransform == nullptr)
        return;
    Snapshot& snap = *s_capture.snap;
    if (!snap.hasBody)
    {
        // RenderPartObject transforms every body part with the character's bones, at its place and scale.
        CopyBones(o->BoneTransform, snap.body.bones);
        VectorCopy(o->Position, snap.body.origin);
        snap.body.bodyScale = o->Scale;
        snap.body.skinScale = 0.f;
        snap.body.boneScale = 1.f;
        snap.body.translate = translate;
        snap.hasBody = true;
    }
    if (snap.partCount >= MAX_BODYPART)
        return;
    const int drawn = Render::Items::Display::GetDrawnModel(model, level);
    if (!IsModel(drawn))
        return;
    // RenderPartObject draws every body part with the character's blend mesh and none hidden.
    snap.parts[snap.partCount++] = {drawn, {o->BlendMesh, -1, ClothMeshOf(drawn)}};
    snap.valid = true;
}

void NoteLinkedItem(const OBJECT* o, int model)
{
    s_capture.lastLinked = -1;
    if (s_capture.object == nullptr || s_capture.object != o || o->EnableShadow || !IsModel(model))
        return;
    Snapshot& snap = *s_capture.snap;
    if (snap.linkedCount >= MaxLinked)
        return;
    const BMD& b = Models[model];
    if (b.m_pCurrentBoneTransform == nullptr)
        return;
    // Linked items are transformed in the shared BoneTransform palette, MAX_BONES long.
    Pose& pose = snap.linked[snap.linkedCount];
    CopyBones(b.m_pCurrentBoneTransform, pose.bones);
    VectorCopy(b.BodyOrigin, pose.origin);
    pose.bodyScale = b.BodyScale;
    pose.skinScale = b.m_LastSkinScale;
    pose.boneScale = b.m_LastBoneScale;
    pose.translate = b.m_LastTranslate;
    snap.linkedItems[snap.linkedCount] = {model, {}};
    s_capture.lastLinked = snap.linkedCount++;
}

void NoteLinkedItemDrawn(const OBJECT* o, const OBJECT* item)
{
    if (s_capture.object == nullptr || s_capture.object != o || item == nullptr || s_capture.lastLinked < 0)
        return;
    MeshLook& look = s_capture.snap->linkedItems[s_capture.lastLinked].look;
    look.blendMesh = item->BlendMesh;
    look.hiddenMesh = item->HiddenMesh;
    s_capture.lastLinked = -1;
}

void NoteMount(const OBJECT* mount)
{
    if (mount == nullptr || mount->Owner == nullptr || !IsRidingMount(mount->Type))
        return;
    Entry* const entry = FindActive(mount->Owner);
    if (entry == nullptr || entry->pendingAt != WorldTime || entry->pendingSlot < 0)
        return;
    Snapshot& snap = entry->snaps[entry->pendingSlot];
    if (!snap.valid || snap.hasMount)
        return;
    const BMD& b = Models[mount->Type];
    if (b.m_pCurrentBoneTransform == nullptr)
        return;
    // Mounts are transformed in world space (no translate), in the shared palette or their own, both MAX_BONES long.
    CopyBones(b.m_pCurrentBoneTransform, snap.mount.bones);
    VectorCopy(b.BodyOrigin, snap.mount.origin);
    snap.mount.bodyScale = b.BodyScale;
    snap.mount.skinScale = b.m_LastSkinScale;
    snap.mount.boneScale = b.m_LastBoneScale;
    snap.mount.translate = b.m_LastTranslate;
    snap.mountDrawn = {mount->Type, {mount->BlendMesh, mount->HiddenMesh, -1}};
    snap.hasMount = true;
}

void Render()
{
    s_capture = {};
    if (SceneFlag != MAIN_SCENE)
    {
        DropAll();
        return;
    }
    DrawCopies();
    Choose();
}

bool OtherEchoesOn()
{
    const std::optional<bool>& saved = GameConfig::GetInstance().GetOtherEchoes();
    return saved.has_value() ? *saved : !IsPhone();
}

void SetOtherEchoes(bool shown)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetOtherEchoes(shown);
    config.Save();
}
} // namespace Render::Effects::SeasonEcho
