#include <doctest.h>

#include "Render/Effects/FlipbookBlend.h"

#include <limits>

namespace
{
namespace Flipbook = Render::Effects::FlipbookBlend;
using doctest::Approx;
} // namespace

TEST_CASE("a phase between two frames blends towards the next [render][flipbook_blend]")
{
    const Flipbook::Frames frames = Flipbook::Split(2.25f, 16);
    CHECK(frames.frame == 2);
    CHECK(frames.next == 3);
    CHECK(frames.weight == Approx(0.25f));
}

TEST_CASE("the frame is the one the particle had before, cut the same way [render][flipbook_blend]")
{
    // As `o->Frame = (20 - o->LifeTime) / 2;` with a float LifeTime.
    for (float life = 20.0f; life >= 0.0f; life -= 0.37f)
    {
        const float phase = (20.0f - life) / 2.0f;
        const int frame = static_cast<int>(phase);
        CHECK(Flipbook::Split(phase, 16).frame == frame);
    }
}

TEST_CASE("the last frame holds [render][flipbook_blend]")
{
    const Flipbook::Frames last = Flipbook::Split(3.6f, 4);
    CHECK(last.frame == 3);
    CHECK(last.next == 3);
    CHECK(last.weight == 0.0f);
    const Flipbook::Frames past = Flipbook::Split(7.5f, 4);
    CHECK(past.frame == 3);
    CHECK(past.next == 3);
    CHECK(past.weight == 0.0f);
    // The frame before the last still blends into it.
    CHECK(Flipbook::Split(2.5f, 4).next == 3);
}

TEST_CASE("no phase, a negative one or a one-frame sheet never blends [render][flipbook_blend]")
{
    CHECK(Flipbook::Split(Flipbook::kNoPhase, 16).weight == 0.0f);
    CHECK(Flipbook::Split(-0.5f, 16).weight == 0.0f);
    CHECK(Flipbook::Split(-0.5f, 16).frame == 0);
    CHECK(Flipbook::Split(0.5f, 1).weight == 0.0f);
    CHECK(Flipbook::Split(0.5f, 0).weight == 0.0f);
}

TEST_CASE("a phase that is not a number or too large to cut never blends [render][flipbook_blend]")
{
    const Flipbook::Frames nan = Flipbook::Split(std::numeric_limits<float>::quiet_NaN(), 16);
    CHECK(nan.frame == 0);
    CHECK(nan.weight == 0.0f);
    const Flipbook::Frames huge = Flipbook::Split(1.0e30f, 16);
    CHECK(huge.frame == 15);
    CHECK(huge.next == 15);
    CHECK(huge.weight == 0.0f);
    const Flipbook::Frames infinite = Flipbook::Split(std::numeric_limits<float>::infinity(), 4);
    CHECK(infinite.frame == 3);
    CHECK(infinite.weight == 0.0f);
    CHECK(Flipbook::Split(-std::numeric_limits<float>::infinity(), 4).weight == 0.0f);
}

TEST_CASE("a whole phase shows its frame alone [render][flipbook_blend]")
{
    const Flipbook::Frames frames = Flipbook::Split(5.0f, 16);
    CHECK(frames.frame == 5);
    CHECK(frames.weight == 0.0f);
}
