#include <doctest.h>

#include "Render/Models/AnimationSmoothing.h"

#include <cmath>

namespace
{
namespace Smooth = Render::AnimationSmoothing;

float Length(const float q[4])
{
    return std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

// A unit quaternion turning by angle (radians) about the z axis.
void AboutZ(float angle, float out[4])
{
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = std::sin(angle * 0.5f);
    out[3] = std::cos(angle * 0.5f);
}
} // namespace

TEST_CASE("anim spline: the keys themselves at s = 0 and s = 1 [render][anim]")
{
    CHECK(Smooth::LimitedHermite(0.0f, 1.0f, 3.0f, 4.0f, 0.0f) == doctest::Approx(1.0f));
    CHECK(Smooth::LimitedHermite(0.0f, 1.0f, 3.0f, 4.0f, 1.0f) == doctest::Approx(3.0f));
    CHECK(Smooth::LimitedHermite(9.0f, -2.0f, 5.0f, -7.0f, 0.0f) == doctest::Approx(-2.0f));
    CHECK(Smooth::LimitedHermite(9.0f, -2.0f, 5.0f, -7.0f, 1.0f) == doctest::Approx(5.0f));
}

TEST_CASE("anim spline: never leaves the segment between its two keys [render][anim]")
{
    // Steep neighbours on either side would swing a plain Catmull-Rom past the keys.
    const float cases[][4] = {
        {-100.0f, 0.0f, 1.0f, 100.0f}, {100.0f, 0.0f, 1.0f, -100.0f}, {0.0f, 0.0f, 1.0f, 1.0f},
        {5.0f, 1.0f, 0.0f, -50.0f},    {1.0f, 1.0f, 1.001f, 40.0f},   {-3.0f, 2.0f, -2.0f, 3.0f},
    };
    for (const auto& c : cases)
    {
        const float low = std::fmin(c[1], c[2]);
        const float high = std::fmax(c[1], c[2]);
        for (int step = 0; step <= 100; ++step)
        {
            const float s = static_cast<float>(step) / 100.0f;
            const float value = Smooth::LimitedHermite(c[0], c[1], c[2], c[3], s);
            CHECK(value >= low);
            CHECK(value <= high);
        }
    }
}

TEST_CASE("anim spline: moves only forward along its segment [render][anim]")
{
    float previous = Smooth::LimitedHermite(-100.0f, 0.0f, 1.0f, 100.0f, 0.0f);
    for (int step = 1; step <= 100; ++step)
    {
        const float value = Smooth::LimitedHermite(-100.0f, 0.0f, 1.0f, 100.0f, static_cast<float>(step) / 100.0f);
        CHECK(value >= previous);
        previous = value;
    }
}

TEST_CASE("anim spline: two equal keys hold still [render][anim]")
{
    for (int step = 0; step <= 10; ++step)
    {
        const float s = static_cast<float>(step) / 10.0f;
        CHECK(Smooth::LimitedHermite(-5.0f, 2.0f, 2.0f, 9.0f, s) == 2.0f);
    }
}

TEST_CASE("anim spline: evenly spaced keys follow the straight line [render][anim]")
{
    for (int step = 0; step <= 10; ++step)
    {
        const float s = static_cast<float>(step) / 10.0f;
        CHECK(Smooth::LimitedHermite(0.0f, 1.0f, 2.0f, 3.0f, s) == doctest::Approx(1.0f + s));
    }
}

TEST_CASE("anim spline: quaternions stay unit length and hit their keys [render][anim]")
{
    float q0[4], q1[4], q2[4], q3[4];
    AboutZ(-0.4f, q0);
    AboutZ(0.0f, q1);
    AboutZ(0.9f, q2);
    AboutZ(1.1f, q3);
    for (int step = 0; step <= 20; ++step)
    {
        float out[4];
        Smooth::SplineQuaternion(q0, q1, q2, q3, static_cast<float>(step) / 20.0f, out);
        CHECK(Length(out) == doctest::Approx(1.0f).epsilon(1e-5));
    }
    float start[4], end[4];
    Smooth::SplineQuaternion(q0, q1, q2, q3, 0.0f, start);
    Smooth::SplineQuaternion(q0, q1, q2, q3, 1.0f, end);
    for (int i = 0; i < 4; ++i)
    {
        CHECK(start[i] == doctest::Approx(q1[i]));
        CHECK(end[i] == doctest::Approx(q2[i]));
    }
}

TEST_CASE("anim spline: a key stored on the other hemisphere is the same rotation [render][anim]")
{
    float q0[4], q1[4], q2[4], q3[4];
    AboutZ(-0.4f, q0);
    AboutZ(0.0f, q1);
    AboutZ(0.9f, q2);
    AboutZ(1.1f, q3);
    float flipped[4];
    float flippedNext[4];
    for (int i = 0; i < 4; ++i)
    {
        flipped[i] = -q2[i];
        flippedNext[i] = -q3[i];
    }
    for (int step = 0; step <= 10; ++step)
    {
        const float s = static_cast<float>(step) / 10.0f;
        float plain[4], mixed[4];
        Smooth::SplineQuaternion(q0, q1, q2, q3, s, plain);
        Smooth::SplineQuaternion(q0, q1, flipped, flippedNext, s, mixed);
        CHECK(Smooth::RotationStep(plain, mixed) == doctest::Approx(0.0f).epsilon(1e-3));
        // The turn stays the short way: never further from the first key than the second key is.
        CHECK(Smooth::RotationStep(q1, mixed) <= Smooth::RotationStep(q1, q2) + 1e-4f);
    }
}

TEST_CASE("anim keys: a regular step takes its neighbours, clamped at the ends [render][anim]")
{
    const auto middle = Smooth::FindSplineKeys(2, 3, 6, false, false, false);
    CHECK(middle.regular);
    CHECK(middle.prev == 1);
    CHECK(middle.next == 4);

    const auto first = Smooth::FindSplineKeys(0, 1, 6, false, false, false);
    CHECK(first.regular);
    CHECK(first.prev == 0);
    CHECK(first.next == 2);

    const auto last = Smooth::FindSplineKeys(4, 5, 6, false, false, false);
    CHECK(last.regular);
    CHECK(last.prev == 3);
    CHECK(last.next == 5);

    // The step from the last key back to the first stays linear for an action that is no cycle.
    CHECK_FALSE(Smooth::FindSplineKeys(5, 0, 6, false, false, false).regular);
}

TEST_CASE("anim keys: a cycle wraps its neighbours around [render][anim]")
{
    const auto first = Smooth::FindSplineKeys(0, 1, 6, false, false, true);
    CHECK(first.prev == 5);
    const auto last = Smooth::FindSplineKeys(4, 5, 6, false, false, true);
    CHECK(last.next == 0);
    const auto wrap = Smooth::FindSplineKeys(5, 0, 6, false, false, true);
    CHECK(wrap.regular);
    CHECK(wrap.prev == 4);
    CHECK(wrap.next == 1);
}

TEST_CASE("anim keys: locked positions wrap before the last key [render][anim]")
{
    // 6 keys with LockPositions: PlayAnimation shows keys 0..4, so 4 -> 0 is the wrap.
    const auto wrap = Smooth::FindSplineKeys(4, 0, 6, true, false, true);
    CHECK(wrap.regular);
    CHECK(wrap.prev == 3);
    CHECK(wrap.next == 1);
    CHECK_FALSE(Smooth::FindSplineKeys(4, 5, 6, true, false, true).regular);
}

TEST_CASE("anim keys: an action held at its end never wraps [render][anim]")
{
    const auto last = Smooth::FindSplineKeys(4, 5, 6, false, true, true);
    CHECK(last.regular);
    CHECK(last.next == 5);
    CHECK_FALSE(Smooth::FindSplineKeys(5, 0, 6, false, true, true).regular);
}

TEST_CASE("anim keys: short actions, jumps and keys out of range stay linear [render][anim]")
{
    CHECK_FALSE(Smooth::FindSplineKeys(0, 1, 2, false, false, true).regular);
    CHECK_FALSE(Smooth::FindSplineKeys(1, 3, 6, false, false, true).regular);
    CHECK_FALSE(Smooth::FindSplineKeys(3, 3, 6, false, false, true).regular);
    CHECK_FALSE(Smooth::FindSplineKeys(5, 6, 6, false, false, true).regular);
    CHECK_FALSE(Smooth::FindSplineKeys(-1, 0, 6, false, false, true).regular);
}

TEST_CASE("anim cycle: the wrap step must be close to the action's own steps [render][anim]")
{
    CHECK(Smooth::IsCyclic(0.1f, 0.2f));
    CHECK(Smooth::IsCyclic(0.3f, 0.2f));
    CHECK_FALSE(Smooth::IsCyclic(0.31f, 0.2f));
}

TEST_CASE("anim fade: the weight grows from 0 to 1 without going back [render][anim]")
{
    CHECK(Smooth::FadeWeight(0.0f) == 0.0f);
    CHECK(Smooth::FadeWeight(1.0f) == 1.0f);
    float previous = 0.0f;
    for (int step = 0; step <= 100; ++step)
    {
        const float weight = Smooth::FadeWeight(Smooth::FadeProgress(static_cast<double>(step) * 1.6, 160, 0.0f));
        CHECK(weight >= previous);
        CHECK(weight <= 1.0f);
        previous = weight;
    }
    CHECK(previous == 1.0f);
}

TEST_CASE("anim fade: the new action's frame moves the fade on too [render][anim]")
{
    CHECK(Smooth::FadeProgress(0.0, 160, 1.0f) == doctest::Approx(0.5f));
    CHECK(Smooth::FadeProgress(80.0, 160, 0.2f) == doctest::Approx(0.5f));
    CHECK(Smooth::FadeProgress(0.0, 160, 2.5f) == 1.0f);
    CHECK(Smooth::FadeProgress(10.0, 0, 0.0f) == 1.0f);
    CHECK(Smooth::FadeProgress(-5.0, 160, 0.0f) == 0.0f);
}

TEST_CASE("anim fade: ends once the stretched start reaches the action's own frames [render][anim]")
{
    CHECK_FALSE(Smooth::FadeFinished(1.0f, 1.9f, 8));
    CHECK(Smooth::FadeFinished(1.0f, 2.0f, 8));
    CHECK(Smooth::FadeFinished(0.2f, 2.0f, 8));
    // Too short to stretch: by progress alone.
    CHECK_FALSE(Smooth::FadeFinished(0.9f, 1.5f, 2));
    CHECK(Smooth::FadeFinished(1.0f, 0.5f, 2));
}

TEST_CASE("anim fade: the stretched start runs key 0 -> 1 and meets the frames at 2 [render][anim]")
{
    const Smooth::Segment start{3, 7, 4.5f, 0.0f};
    const Smooth::Segment warpedStart = Smooth::FadeTarget(start, 8);
    CHECK(warpedStart.priorAction == 7);
    CHECK(warpedStart.action == 7);
    CHECK(warpedStart.priorFrame == 0.0f);
    CHECK(warpedStart.frame == doctest::Approx(1.0f));

    const Smooth::Segment half{7, 7, 0.0f, 1.0f};
    CHECK(Smooth::FadeTarget(half, 8).frame == doctest::Approx(1.5f));

    // Just before frame 2 the stretched segment is at the end of key 0 -> 1, that is at key 1, where
    // the unstretched segment 1 -> 2 starts at frame 2.
    const Smooth::Segment nearEnd{7, 7, 1.0f, 1.9999f};
    const Smooth::Segment warpedEnd = Smooth::FadeTarget(nearEnd, 8);
    CHECK(static_cast<int>(warpedEnd.frame) == 1);
    CHECK(warpedEnd.frame - 1.0f == doctest::Approx(1.0f).epsilon(1e-3));

    const Smooth::Segment after{7, 7, 2.0f, 2.0f};
    const Smooth::Segment unchanged = Smooth::FadeTarget(after, 8);
    CHECK(unchanged.frame == 2.0f);
    CHECK(unchanged.priorFrame == 2.0f);

    // Too short to stretch: the frames as they are.
    CHECK(Smooth::FadeTarget(start, 2).frame == 0.0f);
    CHECK(Smooth::FadeTarget(start, 2).priorAction == 3);
}

TEST_CASE("anim setting: on only where available, fade clamped to 0..300 ms [render][anim]")
{
    CHECK(Smooth::Resolve(1, true));
    CHECK_FALSE(Smooth::Resolve(1, false));
    CHECK_FALSE(Smooth::Resolve(0, true));
    CHECK_FALSE(Smooth::Resolve(2, true));
    CHECK(Smooth::ClampFadeMs(-10) == 0);
    CHECK(Smooth::ClampFadeMs(160) == 160);
    CHECK(Smooth::ClampFadeMs(5000) == 300);
}
