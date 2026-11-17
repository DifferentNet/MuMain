// soft_particles.hlsli — the body of basic_textured_fog_soft.frag and basic_textured_fog_soft_glow.frag
// (SOFT_PARTICLES_GLOW): basic_textured_fog.frag for the soft effect sprites (Render/Effects/SoftParticles.h,
// Remaster, desktop only), faded where they cut into the world depth (t1, drawn for SSAO and the haze).
// Pairs with soft_particles.vert. FogUniforms mirrors FogUniform; SceneLighting mirrors
// mu::SceneLightingUniform, mu::SunShadowUniform (unused here) and mu::SoftParticlesUniform (std140).
Texture2D tex : register(t0, space2);
SamplerState s : register(s0, space2);
Texture2D<float> worldDepth : register(t1, space2);
SamplerState worldDepthSampler : register(s1, space2);
cbuffer FogUniforms : register(b0, space3)
{
    uint fogEnabled;
    uint alphaDiscardEnabled;
    float alphaThreshold;
    uint alphaToCoverage; // 1: the pipeline turns alpha into multisample coverage
    float fogStart;
    float fogEnd;
    float detailStrength; // terrain_detail.hlsli: 0 off, else 0..1 (ground draws only)
    float detailScale;    // terrain_detail.hlsli: > 0 variant A, < 0 variant B
    float4 fogColor;
};
cbuffer SceneLighting : register(b1, space3)
{
    float4 sceneLightDirection;
    float4 sceneLightColor;
    float4 diffuseCurve;
    float4 rimParameters;
    float4 specularParameters;
    float4 sceneFogColor;
    float4 sceneFogParameters;
    float4 cameraPosition;
    float4x4 sunViewProjection;
    float4 sunShadowParameters;
    float4 sunDirectionAndSoftness;
    float4 sunShadowPenumbra;
    float4 softProjection; // 1 / proj[0][0], 1 / proj[1][1], proj[2][2], proj[3][2]
    float4 softDepth;      // x: 1 / width, y: 1 / height, z: 1 when the world depth is there
};

struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float fogFactor : TEXCOORD2;
    float eyeDepth : TEXCOORD3;
    float softness : TEXCOORD4;
};

// Same curve as skinned_lit.frag.hlsl and mu::EnhancedFogAmount() (SceneLighting.cpp).
float EnhancedFogAmount(float eyeDepth)
{
    const float scaledDistance = max(eyeDepth - sceneFogParameters.x, 0.0) * sceneFogParameters.z;
    return saturate(1.0 - exp(-scaledDistance * scaledDistance));
}

// Render::Effects::SoftParticles::Fade: 0 where the sprite touches the world behind it, 1 from its
// softness in front of it on; whole without the depth, without softness or with nothing behind.
float SoftFade(FSInput input)
{
    if (softDepth.z <= 0.0 || input.softness <= 0.0)
        return 1.0;
    const float depth = worldDepth.SampleLevel(worldDepthSampler, input.pos.xy * softDepth.xy, 0.0);
    if (depth >= 1.0)
        return 1.0;
    const float sceneDistance = softProjection.w / (depth + softProjection.z);
    return saturate((sceneDistance - input.eyeDepth) / input.softness);
}

float4 main(FSInput input) : SV_Target
{
    float4 color = tex.Sample(s, input.uv) * input.color;
    if (alphaToCoverage)
    {
        const float edge = saturate((color.a - alphaThreshold) / max(fwidth(color.a), 0.0001) + 0.5);
        color.a *= edge;
        if (color.a <= 0.0) discard;
    }
    else if (alphaDiscardEnabled && color.a <= alphaThreshold) discard;
    if (fogEnabled && sceneFogColor.w > 0.0)
        color.rgb = lerp(color.rgb, sceneFogColor.rgb, EnhancedFogAmount(input.eyeDepth));
    const float fade = SoftFade(input);
#if SOFT_PARTICLES_GLOW
    // Additive and glow blending add the colour: less of it, less light.
    color.rgb *= fade;
#else
    // Alpha blending: the sprite turns clear, its colour unchanged.
    color.a *= fade;
#endif
    return color;
}
