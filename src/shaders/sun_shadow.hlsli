// sun_shadow.hlsli — the sun's shadow map lookup for the shadowed receiver shaders
// (basic_textured_fog_shadow.frag, skinned_lit_shadow.frag, terrain_lights_shadow.frag). The shadow
// map is the depth the sun sees (shadow_caster.frag pass); SunShadow mirrors mu::SunShadowUniform and
// follows the SceneLighting registers in the same uniform buffer (slot 1, see PushSceneLightingUniforms).
// The includer declares sunShadowPenumbra (Render/Lighting/SunShadowSoftEdges.h) after
// sunDirectionAndSoftness: x 0 keeps the fixed soft edge, else the edge widens away from the caster.
// Defining SUN_SHADOW_FIXED_EDGE 1 before the include always keeps the fixed edge.
Texture2D<float> sunShadowMap : register(t1, space2);
SamplerComparisonState sunShadowSampler : register(s1, space2);

// 16 points of a Poisson disc, spread over about the unit circle.
static const float2 SunShadowDisc[16] = {
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870),
    float2(0.34495938, 0.29387760),   float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),  float2(0.97484398, 0.75648379),  float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),  float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),  float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)};

// 8 points of a Vogel spiral (r = sqrt((i + 0.5) / 8), angle = i * golden angle) for the blocker
// search of the widening edge.
static const float2 SunShadowSearchDisc[8] = {
    float2(0.25000000, 0.00000000),   float2(-0.31929009, 0.29249588), float2(0.04887247, -0.55687654),
    float2(0.40244448, 0.52491756),   float2(-0.73853511, -0.13063646), float2(0.69960493, -0.44503139),
    float2(-0.23400416, 0.87048380),  float2(-0.44627131, -0.85926825)};

// The soft edge's radius in map uv. Fixed edge: `baseRadius`. Widening edge: the search finds the
// casters around the lookup point (nearer the sun than the receiver), and the edge grows with how far
// behind them the receiver lies, up to sunShadowPenumbra.y texels. Never narrower than `baseRadius`;
// with no caster around it stays `baseRadius`, and the comparison taps still decide the shadow.
float SunShadowEdgeRadius(float2 uv, float depth, float2 depthSlope, float texel, float2x2 turn, float baseRadius)
{
#if SUN_SHADOW_FIXED_EDGE
    return baseRadius;
#else
    if (sunShadowPenumbra.x <= 0.0)
        return baseRadius;
    const int mapSize = (int)(1.0 / texel + 0.5);
    float blockerDepthSum = 0.0;
    float blockerCount = 0.0;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        const float2 tap = mul(turn, SunShadowSearchDisc[i]) * sunShadowPenumbra.z * texel;
        const int2 coord = clamp(int2((uv + tap) / texel), int2(0, 0), int2(mapSize - 1, mapSize - 1));
        const float stored = sunShadowMap.Load(int3(coord, 0));
        if (stored < depth + dot(depthSlope, tap) - sunShadowPenumbra.w)
        {
            blockerDepthSum += stored;
            blockerCount += 1.0;
        }
    }
    if (blockerCount < 0.5)
        return baseRadius;
    const float gap = max(depth - blockerDepthSum / blockerCount, 0.0);
    const float widest = max(baseRadius, sunShadowPenumbra.y * texel);
    return min(baseRadius + gap * sunShadowPenumbra.x * texel, widest);
#endif
}

// 0: lit, 1: fully in the sun's shadow. `normal` may be zero (terrain): then no normal offset.
// `screenPosition` (SV_Position.xy) turns the disc per pixel, so a wide edge is a fine grain
// instead of 16 visible copies of the caster.
float SunShadowAmount(float3 worldPosition, float3 normal, float2 screenPosition)
{
    if (sunShadowParameters.w < 0.5)
        return 0.0;
    const float texel = sunShadowParameters.x;
    // Pushes the lookup point off the surface by about one shadow texel to keep acne away.
    const float3 offsetPosition = worldPosition + normal * sunShadowParameters.z;
    const float4 clip = mul(sunViewProjection, float4(offsetPosition, 1.0));
    const float2 ndc = clip.xy;
    const float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    // A few world units of depth bias on top of the caster pass's slope bias.
    const float depth = clip.z - 0.0006;
    // How the receiver's depth changes across the map (receiver plane bias): a wide disc on a
    // slope compares each tap against the surface's own depth there, not the centre's, so the
    // ground does not shadow itself. Taken before any branch, where derivatives are defined.
    const float3 dx = ddx(float3(uv, depth));
    const float3 dy = ddy(float3(uv, depth));
    const float determinant = dx.x * dy.y - dx.y * dy.x;
    float2 depthSlope = abs(determinant) > 1e-12
                            ? float2(dy.y * dx.z - dx.y * dy.z, dx.x * dy.z - dy.x * dx.z) / determinant
                            : float2(0.0, 0.0);
    // A grazing surface would get a huge slope; past this it falls back to the constant bias.
    depthSlope = clamp(depthSlope, -2.0, 2.0);
    const float edge = max(abs(ndc.x), abs(ndc.y));
    if (edge >= 1.0 || clip.z >= 1.0 || clip.z <= 0.0)
        return 0.0;

    // Soft edge: 16 bilinear comparisons on a Poisson disc of the edge radius (the map's softness
    // radius, or wider away from the caster), turned by an interleaved gradient noise angle per pixel.
    const float angle = 6.2831853 * frac(52.9829189 * frac(dot(screenPosition, float2(0.06711056, 0.00583715))));
    float sine, cosine;
    sincos(angle, sine, cosine);
    const float2x2 turn = float2x2(cosine, -sine, sine, cosine);
    const float baseRadius = max(sunDirectionAndSoftness.w, 0.5) * texel;
    const float radius = SunShadowEdgeRadius(uv, depth, depthSlope, texel, turn, baseRadius);
    float lit = 0.0;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        const float2 tap = mul(turn, SunShadowDisc[i]) * radius;
        lit += sunShadowMap.SampleCmpLevelZero(sunShadowSampler, uv + tap, depth + dot(depthSlope, tap));
    }
    const float shadow = 1.0 - lit / 16.0;
    // Fades out towards the border of the shadow box instead of ending in a line.
    return shadow * (1.0 - smoothstep(0.8, 1.0, edge));
}

// The colour scale a shadowed pixel gets: 1 in the sun, 1 - strength in full shadow.
float SunShadowScale(float shadow)
{
    return 1.0 - shadow * sunShadowParameters.y;
}
