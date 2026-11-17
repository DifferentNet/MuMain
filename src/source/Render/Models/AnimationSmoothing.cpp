#include "Render/Models/AnimationSmoothing.h"

#include <cmath>

namespace Render::AnimationSmoothing
{
namespace
{
constexpr int kQuaternionSize = 4;
// Below this squared length a blended quaternion is too short to normalise.
constexpr float kMinQuaternionLengthSq = 1e-12f;

float Dot(const float a[4], const float b[4])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

// q, turned to the hemisphere of reference (the same rotation).
void OnHemisphereOf(const float reference[4], const float q[4], float out[4])
{
    const float sign = Dot(reference, q) < 0.0f ? -1.0f : 1.0f;
    for (int i = 0; i < kQuaternionSize; ++i)
    {
        out[i] = sign * q[i];
    }
}

// A slope that does not point against its segment's step.
float AlongStep(float slope, float step)
{
    return slope * step < 0.0f ? 0.0f : slope;
}
} // namespace

SplineKeys FindSplineKeys(const int priorKey, const int key, const int keyCount, const bool lockPositions,
                          const bool holdAtEnd, const bool cyclic)
{
    SplineKeys keys;
    if (keyCount < kMinSplineKeys || priorKey < 0 || key < 0)
    {
        return keys;
    }
    // The keys a looping action shows: one with locked positions wraps before its last key.
    const int shownKeys = (lockPositions && !holdAtEnd) ? keyCount - 1 : keyCount;
    if (key >= shownKeys || priorKey >= shownKeys)
    {
        return keys;
    }
    const bool wraps = cyclic && !holdAtEnd && shownKeys >= kMinSplineKeys;
    if (key == priorKey + 1)
    {
        keys.regular = true;
        keys.prev = priorKey > 0 ? priorKey - 1 : (wraps ? shownKeys - 1 : priorKey);
        keys.next = key + 1 < shownKeys ? key + 1 : (wraps ? 0 : key);
        return keys;
    }
    if (wraps && priorKey == shownKeys - 1 && key == 0)
    {
        keys.regular = true;
        keys.prev = priorKey - 1;
        keys.next = 1;
    }
    return keys;
}

float LimitedHermite(const float p0, const float p1, const float p2, const float p3, const float s)
{
    const float step = p2 - p1;
    if (step == 0.0f)
    {
        return p1;
    }
    float m1 = AlongStep((p2 - p0) * 0.5f, step);
    float m2 = AlongStep((p3 - p1) * 0.5f, step);
    // Fritsch-Carlson: slopes within a circle of kMaxSlopeFactor steps keep the curve monotone, so it
    // never leaves [p1, p2].
    const float alpha = m1 / step;
    const float beta = m2 / step;
    const float radiusSq = alpha * alpha + beta * beta;
    if (radiusSq > kMaxSlopeFactor * kMaxSlopeFactor)
    {
        const float scale = kMaxSlopeFactor / std::sqrt(radiusSq);
        m1 *= scale;
        m2 *= scale;
    }

    const float s2 = s * s;
    const float s3 = s2 * s;
    const float h00 = 2.0f * s3 - 3.0f * s2 + 1.0f;
    const float h10 = s3 - 2.0f * s2 + s;
    const float h01 = -2.0f * s3 + 3.0f * s2;
    const float h11 = s3 - s2;
    const float value = h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;
    // Rounding may still step a hair outside.
    return std::clamp(value, std::min(p1, p2), std::max(p1, p2));
}

void SplineQuaternion(const float q0[4], const float q1[4], const float q2[4], const float q3[4], const float s,
                      float out[4])
{
    float a[4];
    float c[4];
    float d[4];
    OnHemisphereOf(q1, q0, a);
    OnHemisphereOf(q1, q2, c);
    OnHemisphereOf(c, q3, d);

    float blended[4];
    for (int i = 0; i < kQuaternionSize; ++i)
    {
        blended[i] = LimitedHermite(a[i], q1[i], c[i], d[i], s);
    }
    const float lengthSq = Dot(blended, blended);
    if (lengthSq < kMinQuaternionLengthSq)
    {
        for (int i = 0; i < kQuaternionSize; ++i)
        {
            out[i] = q1[i];
        }
        return;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    for (int i = 0; i < kQuaternionSize; ++i)
    {
        out[i] = blended[i] * inverseLength;
    }
}

float RotationStep(const float q1[4], const float q2[4])
{
    const float cosine = std::min(1.0f, std::fabs(Dot(q1, q2)));
    return 2.0f * std::acos(cosine);
}

float FadeProgress(const double elapsedMs, const int fadeMs, const float frame)
{
    if (fadeMs <= 0)
    {
        return 1.0f;
    }
    const float byTime = static_cast<float>(std::max(0.0, elapsedMs) / static_cast<double>(fadeMs));
    const float byFrame = std::max(0.0f, frame) / kWarpKeys;
    return std::clamp(std::max(byTime, byFrame), 0.0f, 1.0f);
}

Segment FadeTarget(const Segment& legacy, const int keyCount)
{
    if (!WarpsStart(legacy.frame, keyCount))
    {
        return legacy;
    }
    // Key 0 -> key 1 of the new action, its blend running at half the frame's pace.
    constexpr float firstSegmentEnd = 2.0f;
    const float blend = std::max(0.0f, legacy.frame) / kWarpKeys;
    Segment warped;
    warped.priorAction = legacy.action;
    warped.action = legacy.action;
    warped.priorFrame = 0.0f;
    warped.frame = std::min(1.0f + blend, std::nextafter(firstSegmentEnd, 0.0f));
    return warped;
}

} // namespace Render::AnimationSmoothing
