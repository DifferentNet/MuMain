#include "SdlGpuParticles.h"

#include "SdlGpuPixelFormat.h"

namespace mu
{

namespace
{
SDL_GPUGraphicsPipeline* BuildParticlePipeline(SDL_GPUDevice* device, SDL_GPUShader* vertexShader,
                                               SDL_GPUShader* fragmentShader, SDL_GPUTextureFormat format,
                                               SDL_GPUSampleCount sampleCount)
{
    // Premultiplied: frame = particle + frame * (1 - a); the shader gives a = 0 for the additive
    // kinds. The frame's alpha stays.
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = format;
    colorTarget.blend_state.enable_blend = true;
    colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
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
    // Inside the scene's pass: what the world drew in front hides a particle; it writes no depth.
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = SdlGpuDepthFormat;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = false;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}
} // namespace

bool SdlGpuParticles::CreatePipeline(SDL_GPUDevice* device, SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                     SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount)
{
    DestroyPipeline();
    if (!device || !vertex || !fragment)
    {
        return false;
    }
    m_device = device;
    m_pipeline = BuildParticlePipeline(device, vertex, fragment, frameFormat, sampleCount);
    return IsAvailable();
}

void SdlGpuParticles::DestroyPipeline()
{
    if (m_device && m_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
    }
    m_pipeline = nullptr;
}

bool SdlGpuParticles::IsAvailable() const
{
    return m_pipeline != nullptr;
}

void SdlGpuParticles::Draw(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass,
                           const SdlGpuParticleUniform& uniform, Uint32 vertexCount, const SDL_GPUViewport& viewport,
                           const SDL_Rect& scissor)
{
    if (!IsAvailable() || !pass || vertexCount == 0u || viewport.w <= 0.0f || viewport.h <= 0.0f)
    {
        return;
    }
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, m_pipeline);
    SDL_PushGPUVertexUniformData(commandBuffer, 0, &uniform, sizeof(uniform));
    SDL_DrawGPUPrimitives(pass, vertexCount, 1, 0, 0);
}

} // namespace mu
