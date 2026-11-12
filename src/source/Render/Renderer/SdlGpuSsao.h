#pragma once

// Screen-space ambient occlusion of the Remaster (desktop; Render/PostProcessing/SsaoSetting.h).
// The opaque world's depth is drawn into its own single-sample depth texture (the renderer replays
// the draws into GetDepth()), the occlusion is worked out from it at half size (post_ssao.frag),
// blurred along the depth (post_ssao_blur.frag, across then down), and DrawApply multiplies the
// frame by it, sized back up along the depth (post_ssao_apply.frag). Nothing exists while it is off.

#include "SdlGpuPixelFormat.h"

#include <SDL3/SDL_gpu.h>

namespace mu
{

// Strength 0..1 (0 = off: nothing is allocated or drawn). [Render] Ssao, SsaoStrength.
void SetSsaoSettings(float strength);

// Mirrors SsaoUniforms in post_ssao.frag, post_ssao_blur.frag and post_ssao_apply.frag.
struct SdlGpuSsaoUniform
{
    float projection[4]; // 1 / proj[0][0], 1 / proj[1][1], proj[2][2], proj[3][2]
    float viewport[4];   // the world's viewport in full-size pixels: x, y, width, height
    float fullSize[4];   // full size: 1 / width, 1 / height, width, height
    float sampling[4];   // radius (world units), largest radius (full-size pixels), bias, intensity
    float apply[4];      // fade start, fade end (view distance), strength 0..1, floor
    float blur[4];       // the blur step in uv (x, y), far plane distance, unused
};
static_assert(sizeof(SdlGpuSsaoUniform) == 96);

struct SdlGpuSsaoShaders
{
    SDL_GPUShader* fullscreenVertex = nullptr; // post_fullscreen.vert
    SDL_GPUShader* occlusion = nullptr;        // post_ssao.frag
    SDL_GPUShader* blur = nullptr;             // post_ssao_blur.frag
    SDL_GPUShader* apply = nullptr;            // post_ssao_apply.frag
};

// What one frame's occlusion needs from the scene.
struct SdlGpuSsaoFrame
{
    float projection[4][4]{}; // the world's projection matrix (column-major, glm)
    SDL_GPUViewport viewport{};
    float strength = 0.0f;
    float fadeStart = 0.0f;
    float fadeEnd = 0.0f;
};

class SdlGpuSsao
{
public:
    static constexpr SDL_GPUTextureFormat OcclusionFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    // The occlusion and blur pipelines, and the apply pipeline that draws into frames of
    // `frameFormat` with `sampleCount` samples, inside a pass with a depth target.
    [[nodiscard]] bool CreatePipelines(SDL_GPUDevice* device, const SdlGpuSsaoShaders& shaders,
                                       SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount);
    void DestroyPipelines();
    [[nodiscard]] bool IsAvailable() const;

    // The depth texture (frame size, one sample, sampled) and the two half-size occlusion targets.
    [[nodiscard]] bool EnsureTargets(SdlGpuTargetSize size);
    // The depth the opaque world is drawn into before Render().
    [[nodiscard]] SDL_GPUTexture* GetDepth() const { return m_depth; }

    // The occlusion of GetDepth(), blurred. False when a pass failed: DrawApply then draws nothing.
    bool Render(SDL_GPUCommandBuffer* commandBuffer, const SdlGpuSsaoFrame& frame);
    void DropResult() { m_resultReady = false; }
    // Darkens the target of `pass` (the whole frame) by the occlusion.
    void DrawApply(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass);

    void Release();
    // The depth and occlusion targets; EnsureTargets makes them again.
    void ReleaseTargets();

private:
    bool RunPass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUGraphicsPipeline* pipeline, SDL_GPUTexture* target,
                 SDL_GPUTexture* source);

    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUGraphicsPipeline* m_occlusionPipeline = nullptr;
    SDL_GPUGraphicsPipeline* m_blurPipeline = nullptr;
    SDL_GPUGraphicsPipeline* m_applyPipeline = nullptr;
    SDL_GPUSampler* m_pointSampler = nullptr; // every lookup reads whole texels

    SdlGpuTargetSize m_size{};
    bool m_depthUnsupported = false;
    SDL_GPUTexture* m_depth = nullptr;   // full size, one sample, D32
    SDL_GPUTexture* m_occlusion = nullptr; // half size: R occlusion, G and B the packed depth
    SDL_GPUTexture* m_blurred = nullptr;   // half size: the blur's first step, then the second's source
    SdlGpuSsaoUniform m_uniform{};
    bool m_resultReady = false;
};

} // namespace mu
