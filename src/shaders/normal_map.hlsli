// The Remaster's normal maps (Render/Textures/NormalMaps.h): the texture's relief tilts the
// surface normal per pixel, so the light picks out rivets, seams and engraving on flat polygons.
// The map stores the tilt along u and v in red and green (128 = none); the renderer binds a flat
// 1x1 map for textures without one, which leaves the normal exactly as it was.
// No tangents in the vertices: the tangent frame comes from the screen-space derivatives of the
// position and the texture coordinates (a cotangent frame), mirrored coordinates included.

// The tilt of a normal map texel; (0, 0) for a flat one.
float2 NormalMapTilt(float4 texel)
{
    return (round(texel.rg * 255.0) - 128.0) / 127.0;
}

// `normal` tilted by `tilt` in the frame of `position` and `uv`. The result keeps the length of
// `normal` (the Classic diffuse curve uses the normal as it is interpolated). Derivatives are taken
// before any branch, as the pixel quad must run them together.
float3 PerturbNormal(float3 normal, float3 position, float2 uv, float2 tilt)
{
    const float3 dp1 = ddx(position);
    const float3 dp2 = ddy(position);
    const float2 duv1 = ddx(uv);
    const float2 duv2 = ddy(uv);
    const float3 dp2perp = cross(dp2, normal);
    const float3 dp1perp = cross(normal, dp1);
    const float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    const float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    const float frameScale = max(dot(tangent, tangent), dot(bitangent, bitangent));
    const float tiltSquared = dot(tilt, tilt);
    if (frameScale < 1e-24 || tiltSquared < 1e-8)
        return normal;
    const float lengthOfNormal = length(normal);
    const float3 tilted = (tangent * tilt.x + bitangent * tilt.y) * (rsqrt(frameScale) * lengthOfNormal) +
                          normal * sqrt(saturate(1.0 - tiltSquared));
    const float tiltedLength = length(tilted);
    return tiltedLength > 1e-6 ? tilted * (lengthOfNormal / tiltedLength) : normal;
}
