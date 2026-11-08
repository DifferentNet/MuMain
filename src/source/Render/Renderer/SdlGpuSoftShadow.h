#pragma once

// Soft body shadows.
// The body shadows are drawn into a mask (white, black where a shadow lies), the mask is blurred
// down to half or quarter size, and the frame is darkened by it: colour *= 1 - strength * (1 - mask).

#include "SdlGpuPixelFormat.h"

#include <SDL3/SDL_gpu.h>

#include <array>

namespace mu
{

// Strength 0..1 (0 = the hard shadows), blur levels 1..2. Options > Soft shadows, $softshadow.
void SetSoftShadowSettings(float strength, int levels);

class SdlGpuSoftShadow
{
public:
    static constexpr SDL_GPUTextureFormat MaskFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    // The blur and composite pipelines. The composite draws into frames of `frameFormat` with
    // `sampleCount` samples, inside a pass with a depth target.
    [[nodiscard]] bool CreatePipelines(SDL_GPUDevice* device, SDL_GPUShader* fullscreenVertex,
                                       SDL_GPUShader* blurDown, SDL_GPUShader* blurUp,
                                       SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount);
    void DestroyPipelines();
    [[nodiscard]] bool IsAvailable() const;

    // The mask (frame size and sample count) and the blur levels.
    [[nodiscard]] bool EnsureTargets(SdlGpuTargetSize size, SDL_GPUSampleCount sampleCount);
    // The mask the shadows are drawn into, and the texture it resolves into (null with one sample).
    [[nodiscard]] SDL_GPUTexture* GetMask() const { return m_mask; }
    [[nodiscard]] SDL_GPUTexture* GetMaskResolve() const { return m_maskResolve; }

    // Blurs the mask: `levels` 1 = half size, 2 = down to quarter and back to half.
    void Blur(SDL_GPUCommandBuffer* commandBuffer, int levels);
    void DropResult() { m_result = nullptr; } // the mask pass failed: no darkening this frame
    // Darkens the target of `pass` (the whole frame) by the blurred mask.
    void DrawComposite(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, float strength);

    void Release();

private:
    void ReleaseTargets();
    bool RunPass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUGraphicsPipeline* pipeline, SDL_GPUTexture* target,
                 SdlGpuTargetSize targetSize, SDL_GPUTexture* source, SdlGpuTargetSize sourceSize,
                 bool clearWhite);

    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUGraphicsPipeline* m_invertPipeline = nullptr;
    SDL_GPUGraphicsPipeline* m_downPipeline = nullptr;
    SDL_GPUGraphicsPipeline* m_upPipeline = nullptr;
    SDL_GPUGraphicsPipeline* m_compositePipeline = nullptr;
    SDL_GPUSampler* m_linearSampler = nullptr;

    SdlGpuTargetSize m_size{};
    SDL_GPUSampleCount m_sampleCount = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* m_mask = nullptr;
    SDL_GPUTexture* m_maskResolve = nullptr;
    SDL_GPUTexture* m_half = nullptr;    // the mask at half size
    SDL_GPUTexture* m_quarter = nullptr; // and at quarter size
    SDL_GPUTexture* m_halfBlur = nullptr; // the quarter blurred back up to half
    SDL_GPUTexture* m_result = nullptr;   // what the composite reads
    SdlGpuTargetSize m_resultSize{};
};

} // namespace mu
