// post_ssao_blur.frag.hlsl — one direction of the occlusion's blur (SdlGpuSsao runs it across, then
// down): 4 texels each side, gaussian weights, each tap weighed down by how far its depth lies from
// the centre's, so the blur does not run over an object's edge. Keeps the packed depth.
#include "ssao.hlsli"
Texture2D source : register(t0, space2);
SamplerState pointSampler : register(s0, space2);

static const int kTapsPerSide = 4;
static const float kWeights[5] = {0.2270, 0.1945, 0.1216, 0.0540, 0.0162};
// Taps whose depth differs by this share of the centre's count for nothing.
static const float kDepthTolerance = 0.1;
static const float kSmallestDepth = 0.0001;

float4 main(FSInput input) : SV_Target
{
    const float4 centre = source.SampleLevel(pointSampler, input.uv, 0.0);
    const float centreDepth = max(SsaoUnpackDepth(centre.gb), kSmallestDepth);
    float sum = centre.r * kWeights[0];
    float weightSum = kWeights[0];
    [unroll]
    for (int i = 1; i <= kTapsPerSide; ++i)
    {
        [unroll]
        for (int side = -1; side <= 1; side += 2)
        {
            const float4 tap = source.SampleLevel(pointSampler, input.uv + blurParams.xy * float(i * side), 0.0);
            const float difference = abs(SsaoUnpackDepth(tap.gb) - centreDepth) / centreDepth;
            const float weight = kWeights[i] * saturate(1.0 - difference / kDepthTolerance);
            sum += tap.r * weight;
            weightSum += weight;
        }
    }
    return float4(sum / weightSum, centre.gb, 1.0);
}
