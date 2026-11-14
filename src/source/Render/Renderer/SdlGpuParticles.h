#pragma once

// Ambient air particles of the Remaster (desktop; Render/Effects/ParticlesSetting.h). Stateless
// billboards: one draw per layer with six vertices per particle and no vertex buffer
// (ambient_particles.vert works out each particle from its number), drawn inside the scene's pass
// right after the opaque world (and SSAO and the haze), before joints, effects and the UI. Tested
// against the scene's depth, never writing it.

#include <SDL3/SDL_gpu.h>

namespace mu
{

// Strength 0 to 100 / 70 (0 = off: nothing is drawn). [Render] Particles, ParticlesStrength.
void SetParticlesSettings(float strength);

// Mirrors ParticleUniforms in ambient_particles.vert.
struct SdlGpuParticleUniform
{
    float viewFromWorld[16]; // column-major, as glm stores it
    float projection[16];
    float boxCenter[4];  // xyz: the box's centre in the world, w: phase 0..1
    float boxSize[4];    // xyz: its size, w: the layer's seed
    float travel[4];     // xyz: box lengths travelled per period, w: speed spread 0..1
    float sway[4];       // amplitude xy, amplitude z, sway turns per period, flicker turns per period
    float sizeParams[4]; // smallest, largest radius (world units), smallest, largest radius (pixels)
    float fadeParams[4]; // near fade start, 1 / its length, edge fade start, pixels per unit at distance 1
    float color[4];      // rgb, a: the peak alpha
    float look[4];       // additive 0/1, glow 0/1, flicker depth, rise fade
};
static_assert(sizeof(SdlGpuParticleUniform) == 256);

class SdlGpuParticles
{
public:
    // The pipeline that blends into frames of `frameFormat` with `sampleCount` samples, inside a
    // pass with a depth target (tested, not written).
    [[nodiscard]] bool CreatePipeline(SDL_GPUDevice* device, SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                      SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount);
    void DestroyPipeline();
    [[nodiscard]] bool IsAvailable() const;

    // Draws one layer of `vertexCount` vertices (six per particle) into the target of `pass`.
    void Draw(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, const SdlGpuParticleUniform& uniform,
              Uint32 vertexCount, const SDL_GPUViewport& viewport, const SDL_Rect& scissor);

private:
    SDL_GPUDevice* m_device = nullptr;
    SDL_GPUGraphicsPipeline* m_pipeline = nullptr;
};

} // namespace mu
