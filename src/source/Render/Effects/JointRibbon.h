// JointRibbon.h: the joint trails drawn as smooth ribbons (Remaster, Render/Effects/SkillFx.h,
// Feature::Ribbons; the curve is RibbonSpline.h).
//
// Only the Evil Spirit spirits (BITMAP_JOINT_SPIRIT and SPIRIT2, SubType 0) and Death Stab's wind
// (MODEL_SPEARSKILL, SubType 2) are offered: plain trails with one tail per reference frame, the
// texture's U running along the trail and no sprites of their own. A trail that does not fit (too
// short, too long, a gap in its tails, the frame's budget spent) is left to the classic draw.
// Main thread only (RenderJoints).
#pragma once

#include "Render/Renderer/MuRenderer.h"

#include <vector>

namespace Render::Effects::JointRibbon
{

// The smooth trails drawn in one frame.
struct FrameStats
{
    int joints = 0; // trails drawn as ribbons
    int pieces = 0; // their pieces (a span between two tails is 1 to 8 pieces)
    int quads = 0;  // the quads of those pieces, both faces
};

// Whether the joint's trail may be drawn as a ribbon at all.
[[nodiscard]] bool Offered(const JOINT& joint);

// Appends the joint's ribbon quads to `quads` (same faces, vertex order, V and colour as the classic
// trail) with up to `maxPieces` pieces a span and returns true; returns false and appends nothing
// when the trail is to be drawn as classic.
[[nodiscard]] bool Append(const JOINT& joint, int maxPieces, int quadBudget, std::vector<mu::Vertex3D>& quads);

// The smooth trails of the last whole frame ($fx status).
[[nodiscard]] FrameStats LastFrameStats();

} // namespace Render::Effects::JointRibbon
