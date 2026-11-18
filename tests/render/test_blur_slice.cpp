#include <doctest.h>

#include "Render/Effects/BlurSlice.h"

namespace
{
namespace BlurSlice = Render::Effects::BlurSlice;
using doctest::Approx;
constexpr int kPoses = 10; // CreateWeaponBlur's poses a slice
} // namespace

TEST_CASE("the full slice is the classic ten poses over the whole PlaySpeed [render][blur_slice]")
{
    const BlurSlice::Slice slice = BlurSlice::FullSlice(kPoses);
    CHECK(slice.samples == 10);
    CHECK(slice.share == 1.0f);
}

TEST_CASE("the head slice covers the animation since the kept slice [render][blur_slice]")
{
    // A reference frame since the kept slice (25 FPS, or a stepping frame): the full slice.
    const BlurSlice::Slice whole = BlurSlice::HeadSlice(1.0f, kPoses);
    CHECK(whole.samples == 10);
    CHECK(whole.share == 1.0f);

    // One frame at 144 FPS: about a sixth of the slice and two poses.
    const BlurSlice::Slice sixth = BlurSlice::HeadSlice(0.17f, kPoses);
    CHECK(sixth.samples == 2);
    CHECK(sixth.share == Approx(0.17f));

    // Nothing since: one pose at the weapon now.
    const BlurSlice::Slice none = BlurSlice::HeadSlice(0.0f, kPoses);
    CHECK(none.samples == 1);
    CHECK(none.share == 0.0f);
}

TEST_CASE("the head slice never spans more than the full slice [render][blur_slice]")
{
    const BlurSlice::Slice over = BlurSlice::HeadSlice(1.2f, kPoses);
    CHECK(over.samples == 10);
    CHECK(over.share == 1.0f);
    const BlurSlice::Slice under = BlurSlice::HeadSlice(-0.5f, kPoses);
    CHECK(under.samples == 1);
    CHECK(under.share == 0.0f);
}
