// shadow_caster.frag.hlsl — the sun's shadow map pass: depth only, no colour target. Cutouts
// (leaves, fences, hair) keep their holes: a texel the draw's alpha test drops casts no shadow.
// Pairs with skinned_textured.vert, whose mvp is then the sun's view-projection.
Texture2D tex : register(t0, space2);
SamplerState s : register(s0, space2);
cbuffer FogUniforms : register(b0, space3)
{
    uint fogEnabled;
    uint alphaDiscardEnabled;
    float alphaThreshold;
    uint alphaToCoverage;
    float fogStart;
    float fogEnd;
    float4 fogColor;
};
struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
};

void main(FSInput input)
{
    // Opaque draws cast whole: MU textures often carry an alpha channel that only blending uses.
    // Cutouts and alpha-blended models (the renderer sets alphaDiscardEnabled for those) let the
    // light through their clear texels.
    if (alphaDiscardEnabled == 0u && alphaToCoverage == 0u) return;
    const float alpha = tex.Sample(s, input.uv).a * input.color.a;
    if (alpha <= max(alphaThreshold, 0.25)) discard;
}
