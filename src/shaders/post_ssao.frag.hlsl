// post_ssao.frag.hlsl — screen-space ambient occlusion at half size, from the opaque world's depth
// (SdlGpuSsao). Each pixel rebuilds its view position and normal from the depth and looks at 8
// points around it on a spiral, turned per pixel by interleaved gradient noise; a point above the
// surface and within the radius closes off part of the pixel's hemisphere. Writes the occlusion
// (1: open) and the packed view distance the blur and the apply pass compare against.
#include "ssao.hlsli"
Texture2D<float> depthTexture : register(t0, space2);
SamplerState pointSampler : register(s0, space2);

static const int kTaps = 8;
static const float kGoldenAngle = 2.39996323;
static const float kTwoPi = 6.2831853;
// A distance that keeps the divisions finite.
static const float kEpsilon = 0.0001;

// The view position (x right, y up, z the distance ahead) of full-size pixel `pixel`.
float3 ViewPosition(float2 pixel, float depth)
{
    const float viewDistance = SsaoLinearDepth(depth);
    const float2 ndc = float2((pixel.x - worldViewport.x) / worldViewport.z * 2.0 - 1.0,
                              1.0 - (pixel.y - worldViewport.y) / worldViewport.w * 2.0);
    return float3(ndc * viewDistance * projectionInfo.xy, viewDistance);
}

float3 ViewPositionAt(float2 pixel)
{
    const float depth = depthTexture.SampleLevel(pointSampler, pixel * fullSize.xy, 0.0);
    return ViewPosition(pixel, depth);
}

// The surface's normal from the depth around it: on each axis the neighbour nearer in depth, so an
// object's edge does not bend the normal of the ground behind it. Faces the camera.
float3 ViewNormal(float2 pixel, float3 position)
{
    const float3 left = ViewPositionAt(pixel - float2(1.0, 0.0));
    const float3 right = ViewPositionAt(pixel + float2(1.0, 0.0));
    const float3 up = ViewPositionAt(pixel - float2(0.0, 1.0));
    const float3 down = ViewPositionAt(pixel + float2(0.0, 1.0));
    const float3 alongX = abs(right.z - position.z) < abs(position.z - left.z) ? right - position : position - left;
    const float3 alongY = abs(down.z - position.z) < abs(position.z - up.z) ? down - position : position - up;
    float3 normal = normalize(cross(alongX, alongY));
    if (dot(normal, position) > 0.0)
        normal = -normal;
    return normal;
}

float4 main(FSInput input) : SV_Target
{
    // The full-size texel at the top-left of this half-size pixel's 2x2 block.
    const float2 pixel = floor(input.uv * fullSize.zw - 0.5) + 0.5;
    const float depth = depthTexture.SampleLevel(pointSampler, pixel * fullSize.xy, 0.0);
    const bool outside = pixel.x < worldViewport.x || pixel.y < worldViewport.y ||
                         pixel.x > worldViewport.x + worldViewport.z || pixel.y > worldViewport.y + worldViewport.w;
    if (depth >= 1.0 || outside)
        return float4(1.0, SsaoPackDepth(blurParams.z), 1.0);

    const float3 position = ViewPosition(pixel, depth);
    const float3 normal = ViewNormal(pixel, position);
    const float radius = sampling.x;
    const float radiusPixels = min(radius * worldViewport.w * 0.5 / (position.z * projectionInfo.y), sampling.y);
    const float2 packedDepth = SsaoPackDepth(position.z);
    if (radiusPixels < 1.0)
        return float4(1.0, packedDepth, 1.0);

    const float noise = frac(52.9829189 * frac(dot(input.pos.xy, float2(0.06711056, 0.00583715))));
    const float turn = noise * kTwoPi;
    const float radiusSquared = radius * radius;
    float occlusion = 0.0;
    [unroll]
    for (int i = 0; i < kTaps; ++i)
    {
        const float along = (float(i) + 0.5) / float(kTaps);
        float sine, cosine;
        sincos(float(i) * kGoldenAngle + turn, sine, cosine);
        const float2 tapPixel = floor(pixel + float2(cosine, sine) * (along * radiusPixels)) + 0.5;
        const float3 toTap = ViewPositionAt(tapPixel) - position;
        const float distanceSquared = dot(toTap, toTap);
        const float rise = max(dot(toTap, normal) - sampling.z, 0.0);
        const float falloff = saturate(1.0 - distanceSquared / radiusSquared);
        occlusion += rise / (sqrt(distanceSquared) + kEpsilon) * falloff;
    }
    const float open = saturate(1.0 - 2.0 * sampling.w * occlusion / float(kTaps));
    return float4(open, packedDepth, 1.0);
}
