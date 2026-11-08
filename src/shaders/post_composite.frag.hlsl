// post_composite.frag.hlsl — the processed 3D scene: the scene image with its bloom screen-blended
// on top, colour graded through the map's lookup table. Draws into the frame's destination before
// the UI. The player's screen filters (FXAA, sharpening) run on the scene first.
// CompositeUniforms mirrors mu::SdlGpuCompositeUniform.
Texture2D scene : register(t0, space2);
SamplerState sceneSampler : register(s0, space2);   // linear: texel centres give the texel, FXAA blends between them
Texture2D bloom : register(t1, space2);
SamplerState bloomSampler : register(s1, space2);   // linear: the bloom is half resolution
Texture2D grading : register(t2, space2);
SamplerState gradingSampler : register(s2, space2); // linear: interpolates between table entries
cbuffer CompositeUniforms : register(b0, space3)
{
    float2 inverseTargetSize;
    float bloomScale;     // 0: no bloom
    float gradingStrength; // 0: ungraded; else blend of the graded colour with the ungraded one
    float fxaa;            // 1: FXAA on
    float sharpen;         // 0..1 sharpening strength, 0: off
    float2 padding;
};
struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// The lookup table is a 32x32x32 colour cube stored as 32 slices of 32x32 side by side
// (1024x32): red runs along x inside a slice, green along y, blue picks the slice.
static const float kGradingSize = 32.0;

// Defined after main so the compiled shader declares its textures in binding order (scene,
// bloom, grading): spirv-cross numbers Metal textures in that order.
float3 Grade(float3 color);
bool Fxaa(float2 uv, inout float3 color);
float3 Sharpen(float2 uv, float3 center);

float4 main(FSInput input) : SV_Target
{
    const float2 uv = input.pos.xy * inverseTargetSize;
    float4 color = scene.SampleLevel(sceneSampler, uv, 0.0);
    // Sharpening is skipped on the edges FXAA smoothed: it would undo them. An explicit if, not &&:
    // glslang's HLSL front end evaluates both sides of &&, so FXAA would run with the setting off.
    bool smoothed = false;
    if (fxaa > 0.5)
    {
        smoothed = Fxaa(uv, color.rgb);
    }
    if (sharpen > 0.0 && !smoothed)
    {
        color.rgb = Sharpen(uv, color.rgb);
    }
    if (bloomScale > 0.0)
    {
        // Screen blend: a dark pixel gains about the glow's light, a bright one little, so glows
        // light their surroundings without washing bright surfaces out to white.
        const float3 glow = saturate(bloom.Sample(bloomSampler, uv).rgb * bloomScale);
        color.rgb = 1.0 - (1.0 - color.rgb) * (1.0 - glow);
    }
    if (gradingStrength > 0.0)
    {
        // Render::PostProcessing::BlendGrading mirrors this blend.
        color.rgb = lerp(color.rgb, Grade(color.rgb), gradingStrength);
    }
    return color;
}

float3 Grade(float3 color)
{
    const float lastEntry = kGradingSize - 1.0;
    const float blue = color.b * lastEntry;
    const float lowerSlice = floor(blue);
    const float upperSlice = min(lowerSlice + 1.0, lastEntry);
    const float sliceBlend = blue - lowerSlice;

    // Texel centres of the red/green entry inside one slice.
    const float x = (color.r * lastEntry + 0.5) / (kGradingSize * kGradingSize);
    const float y = (color.g * lastEntry + 0.5) / kGradingSize;
    const float sliceWidth = 1.0 / kGradingSize;
    const float3 lower = grading.Sample(gradingSampler, float2(x + lowerSlice * sliceWidth, y)).rgb;
    const float3 upper = grading.Sample(gradingSampler, float2(x + upperSlice * sliceWidth, y)).rgb;
    return lerp(lower, upper, sliceBlend);
}

float Luma(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

float3 SceneAt(float2 uv)
{
    return scene.SampleLevel(sceneSampler, uv, 0.0).rgb;
}

float LumaAt(float2 uv)
{
    return Luma(SceneAt(uv));
}

// FXAA (after Timothy Lottes' FXAA 3.11 quality path): finds the local edge from
// the luma of the pixel and its 8 neighbours, walks along it to both ends and
// shifts the sample across the edge by how far the pixel is from the nearer end.
// False (colour untouched) where the contrast is too low to be an edge.
static const float kFxaaSteps[8] = {1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0};

bool Fxaa(float2 uv, inout float3 color)
{
    const float2 px = inverseTargetSize;
    const float m = Luma(color);
    const float n = LumaAt(uv + float2(0.0, -px.y));
    const float s = LumaAt(uv + float2(0.0, px.y));
    const float w = LumaAt(uv + float2(-px.x, 0.0));
    const float e = LumaAt(uv + float2(px.x, 0.0));
    const float highest = max(max(max(n, s), max(w, e)), m);
    const float lowest = min(min(min(n, s), min(w, e)), m);
    const float range = highest - lowest;
    if (range < max(0.0312, highest * 0.125))
    {
        return false;
    }

    const float nw = LumaAt(uv + float2(-px.x, -px.y));
    const float ne = LumaAt(uv + float2(px.x, -px.y));
    const float sw = LumaAt(uv + float2(-px.x, px.y));
    const float se = LumaAt(uv + float2(px.x, px.y));

    // Sub-pixel aliasing: how much the pixel differs from its neighbourhood.
    float subpixel = (2.0 * (n + s + w + e) + nw + ne + sw + se) / 12.0;
    subpixel = smoothstep(0.0, 1.0, saturate(abs(subpixel - m) / range));
    subpixel = subpixel * subpixel * 0.75;

    // A horizontal edge changes luma along y.
    const float horizontal = 2.0 * abs(n + s - 2.0 * m) + abs(ne + se - 2.0 * e) + abs(nw + sw - 2.0 * w);
    const float vertical = 2.0 * abs(w + e - 2.0 * m) + abs(nw + ne - 2.0 * n) + abs(sw + se - 2.0 * s);
    const bool isHorizontal = horizontal >= vertical;

    float2 across = isHorizontal ? float2(0.0, px.y) : float2(px.x, 0.0);
    const float positive = isHorizontal ? s : e;
    const float negative = isHorizontal ? n : w;
    float opposite = positive;
    float gradient = abs(positive - m);
    if (abs(negative - m) > gradient)
    {
        across = -across;
        opposite = negative;
        gradient = abs(negative - m);
    }

    // Walk along the edge, half a pixel across it, until the luma leaves the edge's.
    const float2 along = isHorizontal ? float2(px.x, 0.0) : float2(0.0, px.y);
    const float2 edgeUv = uv + across * 0.5;
    const float edgeLuma = (m + opposite) * 0.5;
    const float threshold = gradient * 0.25;
    float2 positiveUv = edgeUv + along;
    float2 negativeUv = edgeUv - along;
    float positiveDelta = LumaAt(positiveUv) - edgeLuma;
    float negativeDelta = LumaAt(negativeUv) - edgeLuma;
    bool positiveEnd = abs(positiveDelta) >= threshold;
    bool negativeEnd = abs(negativeDelta) >= threshold;
    [unroll]
    for (int i = 1; i < 8; ++i)
    {
        if (!positiveEnd)
        {
            positiveUv += along * kFxaaSteps[i];
            positiveDelta = LumaAt(positiveUv) - edgeLuma;
            positiveEnd = abs(positiveDelta) >= threshold;
        }
        if (!negativeEnd)
        {
            negativeUv -= along * kFxaaSteps[i];
            negativeDelta = LumaAt(negativeUv) - edgeLuma;
            negativeEnd = abs(negativeDelta) >= threshold;
        }
    }

    const float positiveDistance = isHorizontal ? positiveUv.x - uv.x : positiveUv.y - uv.y;
    const float negativeDistance = isHorizontal ? uv.x - negativeUv.x : uv.y - negativeUv.y;
    const bool positiveNearer = positiveDistance <= negativeDistance;
    const float nearest = min(positiveDistance, negativeDistance);
    const float endDelta = positiveNearer ? positiveDelta : negativeDelta;
    // Blend only when the nearer end bends away from this pixel's side of the edge.
    float edgeBlend = 0.0;
    if ((endDelta >= 0.0) != (m - edgeLuma >= 0.0))
    {
        edgeBlend = 0.5 - nearest / (positiveDistance + negativeDistance);
    }

    color = SceneAt(uv + across * max(subpixel, edgeBlend));
    return true;
}

// Contrast-adaptive sharpening (after AMD FidelityFX CAS): a sharpening kernel
// whose weight falls where the neighbourhood is already at full contrast, so
// edges do not ring and flat areas are not grainy.
float3 Sharpen(float2 uv, float3 center)
{
    const float2 px = inverseTargetSize;
    const float3 a = SceneAt(uv + float2(-px.x, -px.y));
    const float3 b = SceneAt(uv + float2(0.0, -px.y));
    const float3 c = SceneAt(uv + float2(px.x, -px.y));
    const float3 d = SceneAt(uv + float2(-px.x, 0.0));
    const float3 f = SceneAt(uv + float2(px.x, 0.0));
    const float3 g = SceneAt(uv + float2(-px.x, px.y));
    const float3 h = SceneAt(uv + float2(0.0, px.y));
    const float3 i = SceneAt(uv + float2(px.x, px.y));

    float3 lowest = min(min(min(d, center), min(f, b)), h);
    float3 highest = max(max(max(d, center), max(f, b)), h);
    lowest += min(lowest, min(min(a, c), min(g, i)));
    highest += max(highest, max(max(a, c), max(g, i)));

    const float3 amplitude = sqrt(saturate(min(lowest, 2.0 - highest) / max(highest, 1.0e-4)));
    const float3 weight = amplitude * (-1.0 / lerp(8.0, 5.0, saturate(sharpen)));
    return saturate(((b + d + f + h) * weight + center) / (1.0 + 4.0 * weight));
}
