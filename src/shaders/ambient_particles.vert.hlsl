// ambient_particles.vert.hlsl — the Remaster's ambient air particles (Render/Effects/ParticlesSetting.h).
// No vertex buffer: six vertices per particle, the particle's number is vertexId / 6. Everything about
// a particle comes from a hash of its number and the frame's phase (0..1 over the period), in whole
// turns per period, so the phase's wrap never shows.
cbuffer ParticleUniforms : register(b0, space1)
{
    float4x4 viewFromWorld;
    float4x4 projection;
    float4 boxCenter;  // xyz: the box's centre in the world, w: phase 0..1
    float4 boxSize;    // xyz: its size, w: the layer's seed
    float4 travel;     // xyz: box lengths travelled per period at full speed, w: speed spread 0..1
    float4 sway;       // x: amplitude xy, y: amplitude z (world units), z: sway turns per period, w: flicker turns per period
    float4 sizeParams; // smallest, largest radius (world units), smallest, largest radius (pixels)
    float4 fadeParams; // near fade start, 1 / its length (view distance), edge fade start (share 0..1), pixels per unit at distance 1
    float4 color;      // rgb, a: the peak alpha (already x presence)
    float4 look;       // x: additive 0/1, y: glow 0/1, z: flicker depth 0..1, w: rise fade 0..1
};
struct VSInput { uint vertexId : SV_VertexID; };
struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 look : TEXCOORD2;
};

uint Hash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
float Unit(uint h) { return float(h & 0x00ffffffU) / 16777216.0; }

static const float kTwoPi = 6.28318530718;
static const float2 kCorners[6] = { float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0),
                                    float2(-1.0, 1.0),  float2(1.0, -1.0), float2(1.0, 1.0) };

VSOutput main(VSInput input)
{
    const uint id = input.vertexId / 6u;
    const float2 corner = kCorners[input.vertexId % 6u];
    const float phase = boxCenter.w;

    uint h = Hash(id * 0x9e3779b9U + uint(boxSize.w));
    float u[12];
    for (int i = 0; i < 12; ++i) { h = Hash(h + 0x68bc21ebU); u[i] = Unit(h); }

    // Where it is in the box (shares of its size): its place, its travel, its sway, around the centre.
    // Angles are wrapped to one turn (frac) before 2 pi, so sin/cos never get large arguments.
    const float3 turns = round(travel.xyz * (1.0 - travel.w * u[3]));
    const float swayA = kTwoPi * frac(max(round(sway.z * (0.6 + 0.8 * u[4])), 1.0) * phase + u[5]);
    const float swayB = kTwoPi * frac(max(round(sway.z * (0.6 + 0.8 * u[6])), 1.0) * phase + u[7]);
    const float3 swayOffset = float3(sin(swayA), cos(swayB), sin(swayA + swayB)) * float3(sway.x, sway.x, sway.y);
    float3 rel = float3(u[0], u[1], u[2]) + turns * phase + (swayOffset - boxCenter.xyz) / boxSize.xyz;
    rel = frac(rel + 0.5) - 0.5;
    const float3 world = boxCenter.xyz + rel * boxSize.xyz;

    float4 view = mul(viewFromWorld, float4(world, 1.0));
    const float distance = -view.z;

    // Fades: the box's sides, its top and bottom, the camera, the rise (embers cool on the way up).
    const float edge = max(abs(rel.x), abs(rel.y)) * 2.0;
    const float height = rel.z + 0.5;
    float fade = 1.0 - smoothstep(fadeParams.z, 1.0, edge);
    fade *= smoothstep(0.0, 0.12, height) * (1.0 - smoothstep(0.88, 1.0, height));
    fade *= lerp(1.0, 1.0 - height, look.w);
    fade *= saturate((distance - fadeParams.x) * fadeParams.y);
    const float pulse = 0.5 + 0.5 * sin(kTwoPi * frac(max(round(sway.w * (0.5 + u[9])), 1.0) * phase + u[10]));
    fade *= lerp(1.0, smoothstep(0.35, 0.85, pulse), look.z);

    // Its radius: never under the smallest pixel size (fainter instead), never over the largest.
    const float safeDistance = max(distance, 1.0);
    const float pixels = lerp(sizeParams.x, sizeParams.y, u[8]) * fadeParams.w / safeDistance;
    const float shrink = min(pixels / sizeParams.z, 1.0);
    fade *= shrink * shrink;
    const float radius = clamp(pixels, sizeParams.z, sizeParams.w) * safeDistance / fadeParams.w;

    VSOutput o;
    o.uv = corner;
    o.color = float4(color.rgb, color.a * fade);
    o.look = look.xy;
    if (fade <= 0.002 || distance <= 0.0)
    {
        o.pos = float4(0.0, 0.0, 2.0, 1.0); // outside the clip volume: no pixels
        return o;
    }
    view.xy += corner * radius;
    o.pos = mul(projection, view);
    return o;
}
