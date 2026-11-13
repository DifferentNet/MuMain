#include "SdlGpuHaze.h"

#include "SdlGpuPixelFormat.h"

#include <cmath>

namespace mu
{

namespace
{
constexpr Uint32 FullscreenVertexCount = 3u;

SDL_GPUGraphicsPipeline* BuildBlendPipeline(SDL_GPUDevice* device, SDL_GPUShader* vertexShader,
                                            SDL_GPUShader* fragmentShader, SDL_GPUTextureFormat format,
                                            SDL_GPUSampleCount sampleCount)
{
    // frame = haze * a + frame * (1 - a); the frame's alpha stays.
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = format;
    colorTarget.blend_state.enable_blend = true;
    colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertexShader;
    info.fragment_shader = fragmentShader;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.multisample_state.sample_count = sampleCount;
    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;
    // Drawn inside the frame's pass, which has the frame's depth; no test, no write.
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SdlGpuDepthFormat;
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}
} // namespace

bool SdlGpuHaze::CreatePipeline(SDL_GPUDevice* device, SDL_GPUShader* fullscreenVertex, SDL_GPUShader* fragment,
                                SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount)
{
    DestroyPipeline();
    if (!device || !fullscreenVertex || !fragment)
    {
        return false;
    }
    m_device = device;
    m_pipeline = BuildBlendPipeline(device, fullscreenVertex, fragment, frameFormat, sampleCount);
    if (!m_pointSampler)
    {
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter = SDL_GPU_FILTER_NEAREST;
        sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_pointSampler = SDL_CreateGPUSampler(device, &sampler);
    }
    if (!IsAvailable())
    {
        DestroyPipeline();
        return false;
    }
    return true;
}

void SdlGpuHaze::DestroyPipeline()
{
    if (m_device && m_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
        m_pipeline = nullptr;
    }
}

bool SdlGpuHaze::IsAvailable() const
{
    return m_pipeline && m_pointSampler;
}

void SdlGpuHaze::Draw(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, SDL_GPUTexture* depth,
                      const SdlGpuHazeUniform& uniform, const SDL_GPUViewport& viewport)
{
    if (!IsAvailable() || !pass || !depth || viewport.w <= 0.0f || viewport.h <= 0.0f)
    {
        return;
    }
    const SDL_Rect scissor{static_cast<int>(std::floor(viewport.x)), static_cast<int>(std::floor(viewport.y)),
                           static_cast<int>(std::ceil(viewport.w)), static_cast<int>(std::ceil(viewport.h))};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, m_pipeline);
    const SDL_GPUTextureSamplerBinding binding{depth, m_pointSampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_PushGPUFragmentUniformData(commandBuffer, 0, &uniform, sizeof(uniform));
    SDL_DrawGPUPrimitives(pass, FullscreenVertexCount, 1, 0, 0);
}

void SdlGpuHaze::Release()
{
    DestroyPipeline();
    if (m_device && m_pointSampler)
    {
        SDL_ReleaseGPUSampler(m_device, m_pointSampler);
        m_pointSampler = nullptr;
    }
}

} // namespace mu
