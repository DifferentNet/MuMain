// post_ssao_apply.frag.hlsl — the blurred half-size occlusion sized up to the frame and multiplied
// into it (SdlGpuSsao::DrawApply; the pipeline blends the frame by this colour). The 4 half-size
// texels around a pixel are mixed by their bilinear weights times how close their depth lies to the
// pixel's own, so an edge keeps its side's occlusion. It never goes below the floor, fades out over
// the fade range of the view distance and is scaled by the strength.
#include "ssao.hlsli"
// Declared and first read in binding order (occlusion, then depth): spirv-cross numbers Metal
// textures that way.
Texture2D occlusionTexture : register(t0, space2);
SamplerState occlusionSampler : register(s0, space2);
Texture2D<float> depthTexture : register(t1, space2);
SamplerState depthSampler : register(s1, space2);

static const float kDepthTolerance = 0.1;
static const float kSmallestWeight = 0.001;
static const float kSmallestDepth = 0.0001;

float4 main(FSInput input) : SV_Target
{
    const float2 uv = input.pos.xy * fullSize.xy;
    const float2 halfSize = max(floor(fullSize.zw * 0.5), 1.0);
    const float2 halfPosition = uv * halfSize - 0.5;
    const float2 base = floor(halfPosition);
    const float2 blend = halfPosition - base;
    const float4 tap00 = occlusionTexture.SampleLevel(occlusionSampler, (base + float2(0.5, 0.5)) / halfSize, 0.0);
    const float4 tap10 = occlusionTexture.SampleLevel(occlusionSampler, (base + float2(1.5, 0.5)) / halfSize, 0.0);
    const float4 tap01 = occlusionTexture.SampleLevel(occlusionSampler, (base + float2(0.5, 1.5)) / halfSize, 0.0);
    const float4 tap11 = occlusionTexture.SampleLevel(occlusionSampler, (base + float2(1.5, 1.5)) / halfSize, 0.0);

    const float depth = depthTexture.SampleLevel(depthSampler, uv, 0.0);
    if (depth >= 1.0)
        return float4(1.0, 1.0, 1.0, 1.0);
    const float viewDistance = SsaoLinearDepth(depth);
    const float packedDistance = max(saturate(viewDistance / blurParams.z), kSmallestDepth);

    const float4 bilinear = float4((1.0 - blend.x) * (1.0 - blend.y), blend.x * (1.0 - blend.y),
                                   (1.0 - blend.x) * blend.y, blend.x * blend.y);
    const float4 tapDepths = float4(SsaoUnpackDepth(tap00.gb), SsaoUnpackDepth(tap10.gb),
                                    SsaoUnpackDepth(tap01.gb), SsaoUnpackDepth(tap11.gb));
    const float4 closeness = saturate(1.0 - abs(tapDepths - packedDistance) / (packedDistance * kDepthTolerance));
    const float4 weights = bilinear * max(closeness, kSmallestWeight);
    const float4 occlusions = float4(tap00.r, tap10.r, tap01.r, tap11.r);
    const float open = dot(occlusions, weights) / max(dot(weights, float4(1.0, 1.0, 1.0, 1.0)), kSmallestWeight);

    const float fadeLength = max(applyParams.y - applyParams.x, kSmallestDepth);
    const float fade = 1.0 - saturate((viewDistance - applyParams.x) / fadeLength);
    const float darkened = max(open, applyParams.w);
    const float result = lerp(1.0, darkened, applyParams.z * fade);
    return float4(result, result, result, 1.0);
}
