// ambient_particles.frag.hlsl — one ambient particle: a soft disc, or a small core with a halo (glow).
// Premultiplied output: rgb * a, and a * (1 - additive) for the blend ONE, ONE_MINUS_SRC_ALPHA.
struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 look : TEXCOORD2; // additive, glow
};
float4 main(FSInput input) : SV_Target
{
    const float r2 = dot(input.uv, input.uv);
    if (r2 >= 1.0)
        discard;
    const float s = 1.0 - r2;
    const float soft = s * s;
    const float glow = 0.35 * soft + 0.65 * exp(-14.0 * r2);
    const float a = input.color.a * lerp(soft, glow, input.look.y);
    return float4(input.color.rgb * a, a * (1.0 - input.look.x));
}
