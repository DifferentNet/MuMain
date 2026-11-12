// terrain_water.frag.hlsl — basic_textured_fog.frag for the terrain's water tiles (Remaster, desktop only):
// small moving ripples catch the sun as a highlight and reflect a sky tint, more at a grazing view.
// Pairs with basic_textured_world.vert (the world position the view direction needs). The renderer
// draws a ground water run through it while IMuRenderer::SetTerrainWater is on: detailStrength then
// carries minus the effect's strength (0..1), and the grain (terrain_detail.hlsli) stays off.
// FogUniforms mirrors FogUniform and SceneLighting the start of mu::SceneLightingUniform (std140).
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
    float detailStrength; // < 0 on water draws: minus the water's strength
    float detailScale;    // on water draws: texture repeats per world unit
    float4 fogColor;
};
cbuffer SceneLighting : register(b1, space3)
{
    float4 sceneLightDirection; // towards the light
    float4 sceneLightColor;
    float4 diffuseCurve;
    float4 rimParameters;
    float4 specularParameters;
    float4 sceneFogColor;
    float4 sceneFogParameters;
    float4 cameraPosition;
};

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

// Ripples: two octaves of gradient value noise. The first is fixed to the world, the second moves
// with the water texture (its uv scrolls) and leads, so the pattern runs as the water flows. The
// flow octave repeats with the texture, so it does not jump when the scroll wraps round. It takes u
// from the texture (the scroll runs along u) and v from the world: FaceTexture sways each tile's v
// with the wind on its own, which would break the ripples at the tile edges.
static const float kRippleWorldCell = 35.0;   // world units (a tile is 100)
static const float kRippleFlowCells = 6.0;    // cells per texture repeat (whole)
static const float kRippleWorldWeight = 0.35;
static const float kRippleFlowWeight = 0.65;
static const float kRippleHeight = 14.0;      // how far the ripples tilt the surface
// Sun glints: the game camera looks down at about 45 degrees and rarely towards the sun, so the
// glints come from a light opposite the camera, this far below the view's mirror direction: flat
// water stays dark and only the ripples tilted towards the camera sparkle. The map's sun gives
// whether it is day (its height).
static const float kGlintDrop = 0.45;         // radians below the mirror direction
static const float kGlintMinElevation = 0.10; // radians
static const float kGlintPower = 180.0;
static const float kGlintScale = 0.55;
static const float kSheenPower = 24.0;
static const float kSheenScale = 0.04;
// Sky reflection: this share at a straight-down view, rising towards kSkyMax at a grazing one.
static const float kSkyMin = 0.06;
static const float kSkyMax = 0.32;
static const float kSkyCurve = 1.6;
static const float3 kSkyTint = float3(0.62, 0.74, 0.90);
// How much of the fog colour (the map's horizon) goes into the sky tint.
static const float kSkyFogShare = 0.4;
static const float3 kLuminanceWeights = float3(0.299, 0.587, 0.114);

// A cell's hash; period > 0 wraps the cells round every period cells (the flow octave).
float WaterHash(float2 cell, float period)
{
    if (period > 0.0)
        cell -= period * floor(cell / period);
    const uint2 q = uint2(int2(cell)) * uint2(1597334673u, 3812015801u);
    const uint n = (q.x ^ q.y) * 1597334673u;
    return float(n >> 8) * (1.0 / 16777216.0);
}

// Value noise's gradient at p (per cell), with the smoothstep's analytic derivative.
float2 WaterNoiseGradient(float2 p, float period)
{
    const float2 cell = floor(p);
    const float2 f = p - cell;
    const float2 w = f * f * (3.0 - 2.0 * f);
    const float2 dw = 6.0 * f * (1.0 - f);
    const float a = WaterHash(cell, period);
    const float b = WaterHash(cell + float2(1.0, 0.0), period);
    const float c = WaterHash(cell + float2(0.0, 1.0), period);
    const float d = WaterHash(cell + float2(1.0, 1.0), period);
    return float2((b - a + (a - b - c + d) * w.y) * dw.x, (c - a + (a - b - c + d) * w.x) * dw.y);
}

float EnhancedFogAmount(float eyeDepth)
{
    const float scaledDistance = max(eyeDepth - sceneFogParameters.x, 0.0) * sceneFogParameters.z;
    return saturate(1.0 - exp(-scaledDistance * scaledDistance));
}

float3 SafeNormalize(float3 v)
{
    const float lengthSquared = dot(v, v);
    return lengthSquared > 1e-8 ? v * rsqrt(lengthSquared) : float3(0.0, 0.0, 1.0);
}

float4 main(FSInput input) : SV_Target
{
    float4 color = tex.Sample(s, input.uv) * input.color;
    if (alphaDiscardEnabled && color.a <= alphaThreshold) discard;

    const float strength = saturate(-detailStrength);
    if (strength > 0.0)
    {
        const float3 worldPosition = input.worldPositionAndDynamicLights.xyz;
        const float2 flowUv = float2(input.uv.x, worldPosition.y * detailScale);
        // Slope of the ripples per world unit (MU is z up; the texture's u and v run along x and y).
        const float2 slope = kRippleWorldWeight * WaterNoiseGradient(worldPosition.xy / kRippleWorldCell, 0.0) /
                                 kRippleWorldCell +
                             kRippleFlowWeight * WaterNoiseGradient(flowUv * kRippleFlowCells, kRippleFlowCells) * kRippleFlowCells *
                                 detailScale;
        const float3 normal = SafeNormalize(float3(-slope * kRippleHeight, 1.0));
        const float3 view = SafeNormalize(cameraPosition.xyz - worldPosition);
        const float facing = saturate(dot(normal, view));

        // The vertex light says how lit this water is (dark in shade and at night).
        const float light = saturate(dot(input.color.rgb, kLuminanceWeights) * 1.2);
        float3 sky = kSkyTint;
        if (sceneFogColor.w > 0.0)
            sky = lerp(sky, sceneFogColor.rgb, kSkyFogShare);
        const float skyShare = lerp(kSkyMin, kSkyMax, pow(1.0 - facing, kSkyCurve));
        color.rgb = lerp(color.rgb, sky * light, skyShare * strength);

        const float sunAbove = saturate(SafeNormalize(sceneLightDirection.xyz).z * 4.0);
        const float2 away = length(view.xy) > 1e-4 ? -normalize(view.xy) : float2(1.0, 0.0);
        const float glintElevation = max(asin(saturate(view.z)) - kGlintDrop, kGlintMinElevation);
        const float3 glintLight = float3(away * cos(glintElevation), sin(glintElevation));
        const float3 halfVector = SafeNormalize(glintLight + view);
        const float highlight = saturate(dot(normal, halfVector));
        const float glint = kGlintScale * pow(highlight, kGlintPower) + kSheenScale * pow(highlight, kSheenPower);
        color.rgb += glint * sunAbove * light * sceneLightColor.rgb * strength;
    }

    if (fogEnabled && sceneFogColor.w > 0.0)
        color.rgb = lerp(color.rgb, sceneFogColor.rgb, EnhancedFogAmount(input.eyeDepth));
    return color;
}
