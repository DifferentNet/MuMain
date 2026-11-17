// soft_particles.vert.hlsl — basic_textured.vert for the soft effect sprites (Render/Effects/SoftParticles.h,
// Remaster, desktop only): the Vertex3D normal's x carries the sprite's softness, a world distance, which
// basic_textured_fog_soft[_glow].frag fades the sprite over in front of the world depth.
cbuffer Transform : register(b0, space1) { float4x4 mvp; float fogStart; float fogEnd; float2 fogPad; };
struct VSInput { float3 pos : TEXCOORD0; float2 uv : TEXCOORD1; float4 color : TEXCOORD2; float3 normal : TEXCOORD3; };
struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float fogFactor : TEXCOORD2;
    float eyeDepth : TEXCOORD3;
    float softness : TEXCOORD4;
    float clipDist : SV_ClipDistance0;
};
VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(mvp, float4(input.pos, 1.0));
    o.uv = input.uv;
    o.color = input.color;
    // As basic_textured.vert: the near-plane clip and the linear fog from the eye distance.
    o.clipDist = o.pos.w - 1.0;
    float dist = abs(o.pos.w);
    float range = fogEnd - fogStart;
    o.fogFactor = (range > 0.001) ? saturate((fogEnd - dist) / range) : 1.0;
    o.eyeDepth = dist;
    o.softness = input.normal.x;
    return o;
}
