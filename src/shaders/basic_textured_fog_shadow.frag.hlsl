// basic_textured_fog_shadow.frag.hlsl — basic_textured_fog.frag that also darkens what lies in
// the sun's shadow. Desktop only: terrain and static objects (basic_textured_world.vert) and unlit
// skinned models (skinned_textured.vert) inside the frame's shadow scope, while sun shadows are on.
// FogUniforms mirrors FogUniform; SceneLighting mirrors mu::SceneLightingUniform followed by
// mu::SunShadowUniform (std140).
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
};
#include "sun_shadow.hlsli"
#include "terrain_detail.hlsli"

struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float fogFactor : TEXCOORD2;
    float eyeDepth : TEXCOORD3;
    float3 normal : TEXCOORD4;
    float3 viewVector : TEXCOORD5;
    nointerpolation float4 lightDirectionAndSpecular : TEXCOORD6;
    float4 worldPositionAndDynamicLights : TEXCOORD7;
    float3 worldNormal : TEXCOORD8;
};

// Same curve as skinned_lit.frag.hlsl and mu::EnhancedFogAmount() (SceneLighting.cpp).
float EnhancedFogAmount(float eyeDepth)
{
    const float scaledDistance = max(eyeDepth - sceneFogParameters.x, 0.0) * sceneFogParameters.z;
    return saturate(1.0 - exp(-scaledDistance * scaledDistance));
}

float4 main(FSInput input) : SV_Target
{
    float4 color = tex.Sample(s, input.uv) * input.color;
    color.rgb *= TerrainDetailFactor(input.uv, input.eyeDepth);
    if (alphaToCoverage)
    {
        const float edge = saturate((color.a - alphaThreshold) / max(fwidth(color.a), 0.0001) + 0.5);
        color.a *= edge;
        if (color.a <= 0.0) discard;
    }
    else if (alphaDiscardEnabled && color.a <= alphaThreshold) discard;
    // Unit or zero: a 3D draw passes no normal.
    const float3 normal = input.worldNormal;
    const float normalLength = length(normal);
    const float3 unitNormal = normalLength > 1e-4 ? normal / normalLength : float3(0.0, 0.0, 0.0);
    color.rgb *= SunShadowScale(SunShadowAmount(input.worldPositionAndDynamicLights.xyz, unitNormal, input.pos.xy));
    if (fogEnabled && sceneFogColor.w > 0.0)
        color.rgb = lerp(color.rgb, sceneFogColor.rgb, EnhancedFogAmount(input.eyeDepth));
    return color;
}
