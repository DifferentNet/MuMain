// sun_shadow.hlsli — the sun's shadow map lookup for the shadowed receiver shaders
// (basic_textured_fog_shadow.frag, skinned_lit_shadow.frag). The shadow map is the depth the sun
// sees (shadow_caster.frag pass); SunShadow mirrors mu::SunShadowUniform and follows the
// SceneLighting registers in the same uniform buffer (slot 1, see PushSceneLightingUniforms).
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

    // Soft edge: 16 bilinear comparisons on a Poisson disc of the map's softness radius,
    // turned by an interleaved gradient noise angle per pixel.
    const float radius = max(sunDirectionAndSoftness.w, 0.5) * texel;
    const float angle = 6.2831853 * frac(52.9829189 * frac(dot(screenPosition, float2(0.06711056, 0.00583715))));
    float sine, cosine;
    sincos(angle, sine, cosine);
    const float2x2 turn = float2x2(cosine, -sine, sine, cosine);
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
