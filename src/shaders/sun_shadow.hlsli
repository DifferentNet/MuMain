// sun_shadow.hlsli — the sun's shadow map lookup for the shadowed receiver shaders
// (basic_textured_fog_shadow.frag, skinned_lit_shadow.frag). The shadow map is the depth the sun
// sees (shadow_caster.frag pass); SunShadow mirrors mu::SunShadowUniform and follows the
// SceneLighting registers in the same uniform buffer (slot 1, see PushSceneLightingUniforms).
Texture2D<float> sunShadowMap : register(t1, space2);
SamplerComparisonState sunShadowSampler : register(s1, space2);

// 0: lit, 1: fully in the sun's shadow. `normal` may be zero (terrain): then no normal offset.
float SunShadowAmount(float3 worldPosition, float3 normal)
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
    const float edge = max(abs(ndc.x), abs(ndc.y));
    if (edge >= 1.0 || clip.z >= 1.0 || clip.z <= 0.0)
        return 0.0;

    // Soft edge: a 4x4 grid of bilinear comparisons spread over the map's softness radius.
    const float radius = max(sunDirectionAndSoftness.w, 0.5) * texel;
    float lit = 0.0;
    [unroll]
    for (int y = 0; y < 4; ++y)
    {
        [unroll]
        for (int x = 0; x < 4; ++x)
        {
            const float2 tap = (float2(x, y) - 1.5) * (radius / 1.5);
            lit += sunShadowMap.SampleCmpLevelZero(sunShadowSampler, uv + tap, depth);
        }
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
