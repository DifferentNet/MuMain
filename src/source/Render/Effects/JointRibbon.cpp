#include "stdafx.h"

#include "Render/Effects/JointRibbon.h"

#include "Render/Effects/RibbonSpline.h"
#include "Render/Effects/SkillFx.h"
#include "Render/Renderer/RenderUtils.h"
#include "Render/Textures/ZzzOpenglUtil.h"

#include <array>

namespace Render::Effects::JointRibbon
{

namespace
{
namespace Spline = RibbonSpline;

// The most pieces a span can take (effect level 4) and so the most points a trail can lay.
constexpr int kMaxPiecesPerSpan = 8;
static_assert(kMaxPiecesPerSpan >= SkillFx::RibbonPieceBudget(4), "s_points must hold the most pieces a level allows");
// A trail is drawn as a ribbon from its first span on, so a growing trail never switches from the
// classic draw to the ribbon (its U would jump). Offered keeps kMinRibbonTails for MaxTails.
constexpr int kMinLaidTails = 2;
constexpr int kMaxPoints = 1 + (Spline::kMaxRibbonTails - 1) * kMaxPiecesPerSpan;
constexpr int kVerticesPerQuad = 4;
// Death Stab's wind (MODEL_SPEARSKILL) and the Evil Spirit spirits.
constexpr int kDeathStabWindSubType = 2;
constexpr int kEvilSpiritSubType = 0;

// Main thread only: RenderJoints is the one caller.
std::array<Spline::RibbonPoint, kMaxPoints> s_points;

unsigned int s_statsFrame = 0;
FrameStats s_current;
FrameStats s_last;

// Water maps call RenderJoints twice a frame (MainScene.cpp, before and after the water), so the quad
// budget and the stats cover both passes; they start over when the frame changes.
FrameStats& CurrentFrameStats()
{
    if (s_statsFrame != RENDERED_FRAME)
    {
        s_last = s_statsFrame + 1 == RENDERED_FRAME ? s_current : FrameStats{};
        s_current = FrameStats{};
        s_statsFrame = RENDERED_FRAME;
    }
    return s_current;
}

// The same test as RenderJoints' IsValidTailQuad: a tail never laid is all zero.
bool IsLaidTail(const vec3_t tail[4])
{
    float mid[3];
    for (int axis = 0; axis < 3; ++axis)
    {
        mid[axis] = (tail[0][axis] + tail[1][axis]) * 0.5f;
    }
    return (mid[0] * mid[0] + mid[1] * mid[1] + mid[2] * mid[2]) >= 1.0f;
}

// The laid tails from slot 0 on, or 0 when a laid tail follows a gap (the trail is not one run).
int LeadingLaidTails(const JOINT& joint)
{
    const int last = std::min<int>(joint.NumTails, MAX_TAILS - 1);
    int count = 0;
    while (count <= last && IsLaidTail(joint.Tails[count]))
    {
        ++count;
    }
    for (int slot = count + 1; slot <= last; ++slot)
    {
        if (IsLaidTail(joint.Tails[slot]))
        {
            return 0;
        }
    }
    return count;
}

void AddFace(std::vector<mu::Vertex3D>& quads, const Spline::RibbonPoint& a, const Spline::RibbonPoint& b,
             const int cornerOne, const int cornerTwo, const float vOne, const float vTwo, const std::uint32_t color)
{
    const auto vertex = [color](const Spline::RibbonPoint& point, const int corner, const float v)
    {
        const float* p = point.corner[corner];
        return mu::Vertex3D{p[0], p[1], p[2], 0.f, 0.f, 0.f, point.u, v, color};
    };
    quads.push_back(vertex(a, cornerOne, vOne));
    quads.push_back(vertex(a, cornerTwo, vTwo));
    quads.push_back(vertex(b, cornerTwo, vTwo));
    quads.push_back(vertex(b, cornerOne, vOne));
}

bool HasFace(const JOINT& joint, const int face)
{
    return (joint.RenderFace & face) == face;
}

int FacesOf(const JOINT& joint)
{
    return (HasFace(joint, RENDER_FACE_ONE) ? 1 : 0) + (HasFace(joint, RENDER_FACE_TWO) ? 1 : 0);
}

// The classic faces: one across corners 2 and 3 (V 1 and 0), two across corners 0 and 1 (V 0 and 1).
void EmitQuads(const JOINT& joint, const int points, std::vector<mu::Vertex3D>& quads)
{
    const std::uint32_t color = mu::PackABGR(joint.Light[0], joint.Light[1], joint.Light[2], 1.f);
    const bool faceOne = HasFace(joint, RENDER_FACE_ONE);
    const bool faceTwo = HasFace(joint, RENDER_FACE_TWO);
    for (int i = 0; i + 1 < points; ++i)
    {
        if (faceOne)
        {
            AddFace(quads, s_points[i], s_points[i + 1], 2, 3, 1.f, 0.f, color);
        }
        if (faceTwo)
        {
            AddFace(quads, s_points[i], s_points[i + 1], 0, 1, 0.f, 1.f, color);
        }
    }
}
} // namespace

bool Offered(const JOINT& joint)
{
    const bool evilSpirit = (joint.Type == BITMAP_JOINT_SPIRIT || joint.Type == BITMAP_JOINT_SPIRIT2) &&
                            joint.SubType == kEvilSpiritSubType;
    const bool deathStabWind = joint.Type == MODEL_SPEARSKILL && joint.SubType == kDeathStabWindSubType;
    return (evilSpirit || deathStabWind) && !joint.bTileMapping && joint.m_byReverseUV == 0 &&
           joint.MaxTails >= Spline::kMinRibbonTails;
}

bool Append(const JOINT& joint, const int maxPieces, const int quadBudget, std::vector<mu::Vertex3D>& quads)
{
    FrameStats& stats = CurrentFrameStats();
    if (stats.quads >= quadBudget)
    {
        return false;
    }
    const int laid = LeadingLaidTails(joint);
    if (laid < kMinLaidTails || laid > Spline::kMaxRibbonTails)
    {
        return false;
    }
    const float frameFactor = FPS_ANIMATION_FACTOR;
    const float phase = joint.TailFrames + frameFactor;
    const int pieces = std::clamp(maxPieces, 1, kMaxPiecesPerSpan);
    const int points =
        Spline::Build(joint.Tails, laid, joint.MaxTails, phase, frameFactor, pieces, s_points.data(), kMaxPoints);
    const int newQuads = (points - 1) * FacesOf(joint);
    if (points < 2 || stats.quads + newQuads > quadBudget)
    {
        return false;
    }
    quads.reserve(quads.size() + static_cast<std::size_t>(newQuads) * kVerticesPerQuad);
    EmitQuads(joint, points, quads);
    ++stats.joints;
    stats.pieces += points - 1;
    stats.quads += newQuads;
    return true;
}

FrameStats LastFrameStats()
{
    if (s_statsFrame == RENDERED_FRAME)
    {
        return s_last;
    }
    return s_statsFrame + 1 == RENDERED_FRAME ? s_current : FrameStats{};
}

} // namespace Render::Effects::JointRibbon
