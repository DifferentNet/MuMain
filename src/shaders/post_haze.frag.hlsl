// post_haze.frag.hlsl — distance haze and low ground fog over the opaque world (SdlGpuHaze; the
// pipeline blends the frame by this colour's alpha). From the world's depth each pixel gets its view
// distance and world height: the haze grows smoothly from behind the hero to the start of the fog,
// the ground fog thickens low places further away, and both fade out under the fog. Pixels with
// nothing drawn (the sky behind the world) get none. HazeSetting.h computes the same.
cbuffer HazeUniforms : register(b0, space3)
{
    float4 projectionInfo; // 1 / proj[0][0], 1 / proj[1][1], proj[2][2], proj[3][2]
    float4 worldViewport;  // the world's viewport in full-size pixels: x, y, width, height
    float4 fullSize;       // 1 / width, 1 / height, width, height
    float4 worldRowX;      // the rows of world-from-view (view space looks down -z)
    float4 worldRowY;
    float4 worldRowZ;
    float4 hazeColor;      // rgb, w = the largest amount
    float4 distanceParams; // start, 1 / (end - start), fog fade start, 1 / (fog fade end - start)
    float4 groundParams;   // reference height, 1 / layer height, ground fog, haze (both x strength)
    float4 sunParams;      // towards the sun (x, y), how much brighter towards it, unused
};
struct FSInput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
Texture2D<float> depthTexture : register(t0, space2);
SamplerState depthSampler : register(s0, space2);

// A length that keeps the division finite.
static const float kSmallestLength = 0.0001;

float4 main(FSInput input) : SV_Target
{
    const float2 pixel = input.pos.xy;
    const float depth = depthTexture.SampleLevel(depthSampler, pixel * fullSize.xy, 0.0);
    if (depth >= 1.0)
        return float4(0.0, 0.0, 0.0, 0.0);
    const float viewDistance = projectionInfo.w / (depth + projectionInfo.z);
    const float share = saturate((viewDistance - distanceParams.x) * distanceParams.y);
    if (share <= 0.0)
        return float4(0.0, 0.0, 0.0, 0.0);

    const float2 ndc = float2((pixel.x - worldViewport.x) / worldViewport.z * 2.0 - 1.0,
                              1.0 - (pixel.y - worldViewport.y) / worldViewport.w * 2.0);
    const float4 viewPosition = float4(ndc * viewDistance * projectionInfo.xy, -viewDistance, 1.0);
    const float worldZ = dot(worldRowZ, viewPosition);
    const float2 rayXY = float2(dot(worldRowX.xyz, viewPosition.xyz), dot(worldRowY.xyz, viewPosition.xyz));

    const float distanceAmount = groundParams.w * share * share * (3.0 - 2.0 * share);
    const float low = saturate(1.0 - (worldZ - groundParams.x) * groundParams.y);
    const float groundAmount = groundParams.z * low * low * sqrt(share);
    const float fogFadeOut = 1.0 - saturate((viewDistance - distanceParams.z) * distanceParams.w);
    const float both = 1.0 - (1.0 - distanceAmount) * (1.0 - groundAmount);
    const float amount = min(both * fogFadeOut, hazeColor.w);

    const float sunFacing = saturate(dot(rayXY / max(length(rayXY), kSmallestLength), sunParams.xy));
    return float4(hazeColor.rgb * (1.0 + sunParams.z * sunFacing), amount);
}
