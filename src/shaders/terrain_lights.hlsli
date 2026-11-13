// terrain_lights.hlsli — the body of terrain_lights.frag and terrain_lights_shadow.frag (Remaster, desktop
// only; Render/Terrain/TorchLights.h): basic_textured_fog.frag (or basic_textured_fog_shadow.frag with
// TERRAIN_LIGHTS_SUN_SHADOW) for the terrain's torch-lit ground draws. The frame's selected map lights are
// taken out of the vertex colour (the vertex's torch part, terrain_lights.vert's `normal`) and added back
// per pixel as smooth round pools on the ground, so the light no longer shows the grid's diamonds.
// Pairs with terrain_lights.vert. FogUniforms mirrors FogUniform; SceneLighting mirrors
// mu::SceneLightingUniform, mu::SunShadowUniform and mu::TerrainLightsUniform (std140, desktop block).
Texture2D tex : register(t0, space2);
SamplerState s : register(s0, space2);
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
    float4 sunShadowParameters;     // x: 1 / map size, y: strength, z: normal offset, w: 1 when on
    float4 sunDirectionAndSoftness; // xyz: towards the sun, w: soft edge radius in texels
    float4 terrainLightHeader;               // x: the number of lights (0..16)
    float4 terrainLightPositionAndRadius[16]; // xy: world position, w: the pool's radius
    float4 terrainLightColor[16];             // rgb: colour * weight * gain
};
#if TERRAIN_LIGHTS_SUN_SHADOW
#include "sun_shadow.hlsli"
#endif
#include "terrain_detail.hlsli"

struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float fogFactor : TEXCOORD2;
    float eyeDepth : TEXCOORD3;
    float3 normal : TEXCOORD4; // the vertex's torch part (0..1 per channel)
    float3 viewVector : TEXCOORD5;
    nointerpolation float4 lightDirectionAndSpecular : TEXCOORD6;
    float4 worldPositionAndDynamicLights : TEXCOORD7;
    float3 worldNormal : TEXCOORD8;
};

static const int kTerrainLightCount = 16;

// Same curve as skinned_lit.frag.hlsl and mu::EnhancedFogAmount() (SceneLighting.cpp).
float EnhancedFogAmount(float eyeDepth)
{
    const float scaledDistance = max(eyeDepth - sceneFogParameters.x, 0.0) * sceneFogParameters.z;
    return saturate(1.0 - exp(-scaledDistance * scaledDistance));
}

// The selected lights at a ground point: colour * smooth(1 - d / radius), d the horizontal distance.
float3 TorchLighting(float3 worldPosition)
{
    float3 light = float3(0.0, 0.0, 0.0);
    const int count = min((int)terrainLightHeader.x, kTerrainLightCount);
    for (int i = 0; i < count; ++i)
    {
        const float4 positionAndRadius = terrainLightPositionAndRadius[i];
        const float2 offset = worldPosition.xy - positionAndRadius.xy;
        const float distanceSquared = dot(offset, offset);
        const float radius = positionAndRadius.w;
        if (radius <= 0.0 || distanceSquared >= radius * radius)
            continue;
        const float t = 1.0 - sqrt(distanceSquared) / radius;
        light += terrainLightColor[i].rgb * (t * t * (3.0 - 2.0 * t));
    }
    return light;
}

float4 main(FSInput input) : SV_Target
{
    const float4 texel = tex.Sample(s, input.uv);
    // The vertex light without the selected lights' part.
    const float3 baseLight = max(input.color.rgb - input.normal, float3(0.0, 0.0, 0.0));
    float shadowScale = 1.0;
#if TERRAIN_LIGHTS_SUN_SHADOW
    // Terrain passes no normal: no normal offset.
    shadowScale = SunShadowScale(SunShadowAmount(input.worldPositionAndDynamicLights.xyz, float3(0.0, 0.0, 0.0),
                                                 input.pos.xy));
#endif
    // The sun's shadow darkens the vertex light only; the torch pools light the shadowed ground too.
    const float3 lit = saturate(baseLight * shadowScale + TorchLighting(input.worldPositionAndDynamicLights.xyz));
    float4 color = float4(texel.rgb * lit, texel.a * input.color.a);
    color.rgb *= TerrainDetailFactor(input.uv, input.eyeDepth);
    if (alphaToCoverage)
    {
        const float edge = saturate((color.a - alphaThreshold) / max(fwidth(color.a), 0.0001) + 0.5);
        color.a *= edge;
        if (color.a <= 0.0) discard;
    }
    else if (alphaDiscardEnabled && color.a <= alphaThreshold) discard;
    if (fogEnabled && sceneFogColor.w > 0.0)
        color.rgb = lerp(color.rgb, sceneFogColor.rgb, EnhancedFogAmount(input.eyeDepth));
    return color;
}
