#pragma once

#include <string>
#include <string_view>

// Graphics cards without WebGPU's clip-distances feature (ARM Mali: the Pixel 9 and many other
// Android phones) can't run the vertex shaders that cut away what is closer than 1 unit to the
// camera with a clip distance. The web build then loads "soft clip" copies of those shaders and of
// the fragment shaders they pair with (tools/web/wgsl-soft-clip.mjs): the vertex shader passes the
// value on and the fragment shader discards the pixel. The picture is the same.
namespace Render::SoftClip
{
inline constexpr std::string_view kBlobSuffix = "_softclip";

// Whether shader `name`.`stage` ("basic_textured", "vert") has a soft clip copy.
[[nodiscard]] bool HasVariant(std::string_view name, std::string_view stage);

// The blob to load for `name`.`stage`: its soft clip copy when `softClip` is on and it has one.
[[nodiscard]] std::string BlobName(std::string_view name, std::string_view stage, bool softClip);
} // namespace Render::SoftClip
