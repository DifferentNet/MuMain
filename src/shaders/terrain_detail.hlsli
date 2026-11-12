// terrain_detail.hlsli — fine grain over the ground tiles near the camera (Remaster, desktop only).
// Included by basic_textured_fog.frag and basic_textured_fog_shadow.frag after their tex, s and
// FogUniforms cbuffer, whose detailStrength and detailScale the renderer sets for the terrain's ground draws
// only (IMuRenderer::SetTerrainDetail); every other draw has detailStrength 0 and keeps its colour.
//   detailScale > 0: variant A, the tile texture itself sampled detailScale times denser, as a ratio
//                    to the texture's mean brightness.
//   detailScale < 0: variant B, hash value noise on uv * |detailScale|.

// Eye distances (MU units, a tile is 100): full grain up to the first, none past the second.
// The game camera sits about 1300 (zoom 0) to 2300 (zoom 7) units from the hero, 1735 by default
// (Camera/OrbitalCamera.h), so the ground around the hero is at about that eye depth.
static const float kTerrainDetailFullDepth = 2400.0;
static const float kTerrainDetailFadeDepth = 3800.0;

// Variant A: the dense copy's brightness over the texture's mean, kept within these bounds.
static const float kSelfDetailMinRatio = 0.6;
static const float kSelfDetailMaxRatio = 1.4;
// A level past the texture's smallest mip (clamped to it): about its mean colour.
static const float kSelfDetailMeanMipLevel = 16.0;
// Floor of that mean, so a near-black texture doesn't blow the ratio up.
static const float kSelfDetailMinMean = 0.02;

// Variant B: three octaves of value noise; each next octave this much denser and with this weight.
static const float kNoiseOctave2Frequency = 2.31;
static const float kNoiseOctave3Frequency = 5.17;
static const float kNoiseOctave1Weight = 0.5;
static const float kNoiseOctave2Weight = 0.3;
static const float kNoiseOctave3Weight = 0.2;
// Peak to peak brightness change of the noise at full strength (0.25: about +-12 % at the extremes).
static const float kNoiseContrast = 0.25;
// An octave fades to its mean once its cells shrink from this many cells per pixel to the next.
static const float kNoiseAliasStart = 0.5;
static const float kNoiseAliasEnd = 1.0;

static const float3 kLuminanceWeights = float3(0.299, 0.587, 0.114);

// 1 near the camera, 0 far away.
float TerrainDetailFade(float eyeDepth)
{
    return 1.0 - smoothstep(kTerrainDetailFullDepth, kTerrainDetailFadeDepth, eyeDepth);
}

// uvDx, uvDy: the uv's screen derivatives, taken by the caller outside any branch.
float TerrainSelfDetail(float2 uv, float2 uvDx, float2 uvDy, float scale)
{
    const float dense = dot(tex.SampleGrad(s, uv * scale, uvDx * scale, uvDy * scale).rgb, kLuminanceWeights);
    const float mean = dot(tex.SampleLevel(s, uv, kSelfDetailMeanMipLevel).rgb, kLuminanceWeights);
    return clamp(dense / max(mean, kSelfDetailMinMean), kSelfDetailMinRatio, kSelfDetailMaxRatio);
}

// A value in [0, 1) for an integer lattice cell.
float TerrainNoiseHash(float2 cell)
{
    const uint2 q = uint2(int2(cell)) * uint2(1597334673u, 3812015801u);
    const uint n = (q.x ^ q.y) * 1597334673u;
    return float(n >> 8) * (1.0 / 16777216.0);
}

float TerrainValueNoise(float2 p)
{
    const float2 cell = floor(p);
    const float2 f = p - cell;
    const float2 w = f * f * (3.0 - 2.0 * f);
    const float a = TerrainNoiseHash(cell);
    const float b = TerrainNoiseHash(cell + float2(1.0, 0.0));
    const float c = TerrainNoiseHash(cell + float2(0.0, 1.0));
    const float d = TerrainNoiseHash(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, w.x), lerp(c, d, w.x), w.y);
}

// One octave, faded to the noise mean (0.5) where its cells get smaller than a pixel.
float TerrainNoiseOctave(float2 p, float cellsPerPixel)
{
    const float alias = smoothstep(kNoiseAliasStart, kNoiseAliasEnd, cellsPerPixel);
    return lerp(TerrainValueNoise(p), 0.5, alias);
}

// About 1 on average, kNoiseContrast peak to peak. uvWidth: fwidth(uv), taken by the caller
// outside any branch.
float TerrainNoiseDetail(float2 uv, float2 uvWidth, float scale)
{
    const float2 p = uv * scale;
    const float2 footprint = uvWidth * scale;
    const float cellsPerPixel = max(footprint.x, footprint.y);
    const float n = kNoiseOctave1Weight * TerrainNoiseOctave(p, cellsPerPixel) +
                    kNoiseOctave2Weight * TerrainNoiseOctave(p * kNoiseOctave2Frequency, cellsPerPixel * kNoiseOctave2Frequency) +
                    kNoiseOctave3Weight * TerrainNoiseOctave(p * kNoiseOctave3Frequency, cellsPerPixel * kNoiseOctave3Frequency);
    return 1.0 + kNoiseContrast * (n - 0.5) * 2.0;
}

// The factor the ground's colour is multiplied by; exactly 1 while detailStrength is 0.
// Called from main outside any branch: the derivatives are taken here, before the branches, and
// the samples below use explicit gradients or levels.
float TerrainDetailFactor(float2 uv, float eyeDepth)
{
    const float2 uvDx = ddx(uv);
    const float2 uvDy = ddy(uv);
    const float2 uvWidth = abs(uvDx) + abs(uvDy); // fwidth(uv)
    if (detailStrength <= 0.0)
        return 1.0;
    const float amount = detailStrength * TerrainDetailFade(eyeDepth);
    if (detailScale > 0.0)
        return lerp(1.0, TerrainSelfDetail(uv, uvDx, uvDy, detailScale), amount);
    return 1.0 + amount * (TerrainNoiseDetail(uv, uvWidth, -detailScale) - 1.0);
}
