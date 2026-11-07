// basic_textured_world.vert.hlsl — basic_textured.vert for 3D draws that receive the sun's shadow.
// Same position, texture coordinates, colour and fog as basic_textured.vert, plus the world position
// the shadow lookup needs. The outputs have skinned_textured.vert's layout, so that the shadowed
// receiver fragment shaders pair with both; the outputs a 3D draw has no use for are zero.
cbuffer Transform : register(b0, space1) { float4x4 mvp; float fogStart; float fogEnd; float2 fogPad; };
// Rows of the affine transform from this draw's vertex space to world space.
cbuffer VertexToWorld : register(b1, space1) { float4 vertexToWorldRows[3]; };
struct VSInput { float3 pos : TEXCOORD0; float2 uv : TEXCOORD1; float4 color : TEXCOORD2; };
struct VSOutput
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
    o.normal = float3(0.0, 0.0, 0.0);
    o.viewVector = float3(0.0, 0.0, 0.0);
    o.lightDirectionAndSpecular = float4(0.0, 0.0, 0.0, 0.0);
    const float4 position = float4(input.pos, 1.0);
    o.worldPositionAndDynamicLights = float4(dot(vertexToWorldRows[0], position), dot(vertexToWorldRows[1], position),
                                             dot(vertexToWorldRows[2], position), 0.0);
    o.worldNormal = float3(0.0, 0.0, 0.0);
    return o;
}
