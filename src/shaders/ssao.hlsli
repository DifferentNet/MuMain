// ssao.hlsli — what the screen-space ambient occlusion passes share (post_ssao.frag,
// post_ssao_blur.frag, post_ssao_apply.frag; Render/PostProcessing/SsaoSetting.h). SsaoUniforms
// mirrors mu::SdlGpuSsaoUniform. The occlusion targets hold the occlusion in R and the pixel's view
// distance over the far plane's, packed in 16 bits, in G and B.
cbuffer SsaoUniforms : register(b0, space3)
{
    float4 projectionInfo; // 1 / proj[0][0], 1 / proj[1][1], proj[2][2], proj[3][2]
    float4 worldViewport;  // the world's viewport in full-size pixels: x, y, width, height
    float4 fullSize;       // 1 / width, 1 / height, width, height
    float4 sampling;       // radius (world units), largest radius (full-size pixels), bias, intensity
    float4 applyParams;    // fade start, fade end (view distance), strength 0..1, floor
    float4 blurParams;     // the blur step in uv (x, y), far plane distance, unused
};
struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// The distance along the view direction of a depth-buffer value (SsaoSetting.h, LinearDepth).
float SsaoLinearDepth(float depth)
{
    return projectionInfo.w / (depth + projectionInfo.z);
}

float2 SsaoPackDepth(float viewDistance)
{
    const float scaled = saturate(viewDistance / blurParams.z) * 255.0;
    const float high = floor(scaled);
    return float2(high / 255.0, scaled - high);
}

float SsaoUnpackDepth(float2 packedDepth)
{
    return (packedDepth.x * 255.0 + packedDepth.y) / 255.0;
}
