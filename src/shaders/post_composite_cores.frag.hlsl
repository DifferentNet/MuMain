// post_composite_cores.frag.hlsl — the processed 3D scene with hot cores (post_composite.frag.hlsl
// with COMPOSITE_CORES): it also reads the effects-only image (the bloom source, t1) and pushes the
// scene toward white where the effects pile up clipped and bright (Render/PostProcessing/HotCores.h).
// Desktop only (Remaster skill effects, SkillFx::Feature::Cores); bloom and grading move to t2, t3.
#define COMPOSITE_CORES 1
#include "post_composite.frag.hlsl"
