#include "SunShadow.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace mu
{

namespace
{

// Unit direction towards the sun, kept at least about 17 degrees above the horizon so that
// shadows stay inside the box; a zero vector means straight above.
glm::vec3 TowardSun(const SunShadowFrame& frame)
{
    const glm::vec3 direction{frame.towardSun[0], frame.towardSun[1], frame.towardSun[2]};
    const float length = glm::length(direction);
    if (!(length > 1e-4f))
    {
        return {0.0f, 0.0f, 1.0f};
    }
    glm::vec3 unit = direction / length;
    if (unit.z < 0.3f)
    {
        unit.z = 0.3f;
        unit = glm::normalize(unit);
    }
    return unit;
}

} // namespace

std::array<float, 16> SunShadowViewProjection(const SunShadowFrame& frame)
{
    const glm::vec3 towardSun = TowardSun(frame);
    const glm::vec3 focus{frame.focus[0], frame.focus[1], frame.focus[2]};
    const float halfDepth = std::max(frame.depthRange, 1.0f) * 0.5f;
    const float halfExtent = std::max(frame.halfExtent, 1.0f);
    // World Z is up; a sun nearly overhead takes world Y as the map's up instead.
    const glm::vec3 up = std::abs(towardSun.z) < 0.99f ? glm::vec3{0.0f, 0.0f, 1.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
    const glm::mat4 view = glm::lookAtRH(focus + towardSun * halfDepth, focus, up);
    glm::mat4 projection = glm::orthoRH_ZO(-halfExtent, halfExtent, -halfExtent, halfExtent, 0.0f, halfDepth * 2.0f);

    // Whole texels: the world origin lands on a texel corner, so the grid does not slide.
    const float mapSize = static_cast<float>(std::max<std::uint32_t>(frame.mapSize, 1u));
    const glm::vec4 origin = projection * view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const float texelsX = origin.x * mapSize * 0.5f;
    const float texelsY = origin.y * mapSize * 0.5f;
    projection[3][0] += (std::round(texelsX) - texelsX) * 2.0f / mapSize;
    projection[3][1] += (std::round(texelsY) - texelsY) * 2.0f / mapSize;

    const glm::mat4 viewProjection = projection * view;
    std::array<float, 16> result{};
    std::memcpy(result.data(), &viewProjection[0][0], sizeof(float) * 16);
    return result;
}

SunShadowUniform PackSunShadowUniform(const SunShadowFrame& frame)
{
    SunShadowUniform uniform{};
    if (!frame.enabled || frame.mapSize == 0u)
    {
        return uniform;
    }
    const std::array<float, 16> viewProjection = SunShadowViewProjection(frame);
    std::copy(viewProjection.begin(), viewProjection.end(), uniform.viewProjection);
    const float mapSize = static_cast<float>(frame.mapSize);
    const float texelWorld = 2.0f * std::max(frame.halfExtent, 1.0f) / mapSize;
    uniform.parameters[0] = 1.0f / mapSize;
    uniform.parameters[1] = std::clamp(frame.strength, 0.0f, 1.0f);
    uniform.parameters[2] = texelWorld * 1.5f;
    uniform.parameters[3] = 1.0f;
    const glm::vec3 towardSun = TowardSun(frame);
    uniform.directionAndSoftness[0] = towardSun.x;
    uniform.directionAndSoftness[1] = towardSun.y;
    uniform.directionAndSoftness[2] = towardSun.z;
    uniform.directionAndSoftness[3] = std::clamp(frame.softness, 0.5f, 8.0f);
    return uniform;
}

std::uint32_t SunShadowMapSize(int level)
{
    switch (level)
    {
    case 1:
        return kSunShadowMapMedium;
    case 2:
        return kSunShadowMapHigh;
    default:
        return 0u;
    }
}

} // namespace mu
