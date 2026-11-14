// terrain_lights.vert.hlsl — basic_textured_world.vert for the terrain's torch-lit ground draws (Remaster,
// desktop only; Render/Terrain/TorchLights.h). Same outputs as basic_textured_world.vert, plus the vertex's
// torch part: the share of its packed colour that comes from the frame's selected map lights (per channel
// 0..1, carried in the vertex normal), passed on in `normal` for terrain_lights.hlsli to take out again.
cbuffer Transform : register(b0, space1) { float4x4 mvp; float fogStart; float fogEnd; float2 fogPad; };
// Rows of the affine transform from this draw's vertex space to world space.
cbuffer VertexToWorld : register(b1, space1) { float4 vertexToWorldRows[3]; };
struct VSInput
{
    float3 pos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 color : TEXCOORD2;
    float3 torchVertexLight : TEXCOORD3;
};
struct VSOutput
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
    float clipDist : SV_ClipDistance0;
};
VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(mvp, float4(input.pos, 1.0));
    o.uv = input.uv;
    o.color = input.color;
    o.clipDist = o.pos.w - 1.0;
    float dist = abs(o.pos.w);
    float range = fogEnd - fogStart;
    o.fogFactor = (range > 0.001) ? saturate((fogEnd - dist) / range) : 1.0;
    o.eyeDepth = dist;
    o.normal = input.torchVertexLight;
    o.viewVector = float3(0.0, 0.0, 0.0);
    o.lightDirectionAndSpecular = float4(0.0, 0.0, 0.0, 0.0);
    const float4 position = float4(input.pos, 1.0);
    o.worldPositionAndDynamicLights = float4(dot(vertexToWorldRows[0], position), dot(vertexToWorldRows[1], position),
                                             dot(vertexToWorldRows[2], position), 0.0);
    o.worldNormal = float3(0.0, 0.0, 0.0);
    return o;
}
