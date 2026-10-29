#include "Render/Renderer/SoftClipShaders.h"

#include <algorithm>
#include <array>

namespace
{
struct ShaderStage
{
    std::string_view name;
    std::string_view stage;
};

// Keep in step with SOFT_CLIP_SHADERS in tools/web/wgsl-soft-clip.mjs. Every pipeline pairs one
// of these vertex shaders with one of these fragment shaders, so both sides switch together.
constexpr std::array<ShaderStage, 6> kSoftClipShaders{{
    {"basic_textured", "vert"},
    {"skinned_textured", "vert"},
    {"skinned_shadow", "vert"},
    {"basic_textured", "frag"},
    {"basic_textured_fog", "frag"},
    {"skinned_lit", "frag"},
}};
} // namespace

bool Render::SoftClip::HasVariant(std::string_view name, std::string_view stage)
{
    return std::any_of(kSoftClipShaders.begin(), kSoftClipShaders.end(),
                       [&](const ShaderStage& shader) { return shader.name == name && shader.stage == stage; });
}

std::string Render::SoftClip::BlobName(std::string_view name, std::string_view stage, bool softClip)
{
    std::string blob(name);
    if (softClip && HasVariant(name, stage))
        blob += kBlobSuffix;
    return blob;
}
