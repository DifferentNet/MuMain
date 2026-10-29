#include <doctest.h>

#include "Render/Renderer/SoftClipShaders.h"

TEST_CASE("soft clip loads the copies of the clip-distance shaders and their fragment shaders [render][soft-clip]")
{
    CHECK(Render::SoftClip::BlobName("basic_textured", "vert", true) == "basic_textured_softclip");
    CHECK(Render::SoftClip::BlobName("skinned_shadow", "vert", true) == "skinned_shadow_softclip");
    CHECK(Render::SoftClip::BlobName("basic_textured", "frag", true) == "basic_textured_softclip");
    CHECK(Render::SoftClip::BlobName("skinned_lit", "frag", true) == "skinned_lit_softclip");
}

TEST_CASE("soft clip leaves other shaders and the normal path alone [render][soft-clip]")
{
    CHECK(Render::SoftClip::BlobName("post_composite", "frag", true) == "post_composite");
    CHECK(Render::SoftClip::BlobName("basic_colored", "vert", true) == "basic_colored");
    CHECK(Render::SoftClip::BlobName("basic_textured", "vert", false) == "basic_textured");
    CHECK_FALSE(Render::SoftClip::HasVariant("skinned_lit", "vert"));
}
