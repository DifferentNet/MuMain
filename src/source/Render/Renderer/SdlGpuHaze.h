#pragma once

// Distance haze and low ground fog of the Remaster (desktop; Render/PostProcessing/HazeSetting.h).
// One full-screen pass over the opaque world's depth (the one SdlGpuSsao draws) blends the haze
// colour over the frame (post_haze.frag), right after the ambient occlusion and before effects,
// particles and the UI. Its pipeline exists only when the world's depth was wanted (SSAO or haze
// on) as the pipelines were built.

#include <SDL3/SDL_gpu.h>

namespace mu
{

// Strength 0 to 100 / 70 (0 = off: nothing is drawn). [Render] Haze, HazeStrength.
void SetHazeSettings(float strength);
// True when this session's renderer can draw it: its pipeline and the world's depth pipelines
// exist only when SSAO or haze was on as they were built (start-up, a change of anti-aliasing).
[[nodiscard]] bool HasHazePipelines();

// Mirrors HazeUniforms in post_haze.frag.
struct SdlGpuHazeUniform
{
    float projection[4];     // 1 / proj[0][0], 1 / proj[1][1], proj[2][2], proj[3][2]
    float viewport[4];       // the world's viewport in full-size pixels: x, y, width, height
    float fullSize[4];       // 1 / width, 1 / height, width, height
    float worldRowX[4];      // the rows of world-from-view (view space looks down -z)
    float worldRowY[4];
    float worldRowZ[4];
    float color[4];          // haze colour, w = the largest amount
    float distance[4];       // start, 1 / (end - start), fog fade start, 1 / (fog fade end - start)
    float ground[4];         // reference height, 1 / layer height, ground fog, haze (both x strength)
    float sun[4];            // towards the sun (x, y), how much brighter towards it, unused
};
static_assert(sizeof(SdlGpuHazeUniform) == 160);

class SdlGpuHaze
{
public:
    // The pipeline that blends into frames of `frameFormat` with `sampleCount` samples, inside a
    // pass with a depth target.
    [[nodiscard]] bool CreatePipeline(SDL_GPUDevice* device, SDL_GPUShader* fullscreenVertex,
                                      SDL_GPUShader* fragment, SDL_GPUTextureFormat frameFormat,
                                      SDL_GPUSampleCount sampleCount);
    void DestroyPipeline();
    [[nodiscard]] bool IsAvailable() const;

    // Blends the haze over the target of `pass` inside `viewport` (the world's), from `depth`
    // (frame size, one sample).
    void Draw(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, SDL_GPUTexture* depth,
              const SdlGpuHazeUniform& uniform, const SDL_GPUViewport& viewport);

    void Release();

private:
    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
    SDL_GPUSampler* m_pointSampler = nullptr; // the depth is read texel by texel
};

} // namespace mu
