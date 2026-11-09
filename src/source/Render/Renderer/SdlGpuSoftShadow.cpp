#include "SdlGpuSoftShadow.h"

#include "SdlGpuPostProcessing.h"
#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebGpuFeatures.h"
#endif

#include <algorithm>

namespace mu
{

namespace
{
constexpr Uint32 FullscreenVertexCount = 3u;

enum class Blend
{
    None,
    Invert,    // target (cleared white) - shader: the mask turned into shadow coverage
    Composite, // frame * (1 - shader); the shader returns strength * coverage
};

SDL_GPUGraphicsPipeline* BuildFullscreenPipeline(SDL_GPUDevice* device, SDL_GPUShader* vertexShader,
                                                 SDL_GPUShader* fragmentShader, SDL_GPUTextureFormat format,
                                                 SDL_GPUSampleCount sampleCount, Blend blend)
{
    const bool composite = blend == Blend::Composite;
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = format;
    // No blend constants: SDL's WebGPU backend passes them in the wrong channel order.
    if (blend == Blend::Invert)
    {
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }
    else if (composite)
    {
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertexShader;
    info.fragment_shader = fragmentShader;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
#ifdef __EMSCRIPTEN__
    info.rasterizer_state.enable_depth_clip = !web::HasDepthClamp();
#endif
    info.multisample_state.sample_count = sampleCount;
    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = composite;
    info.target_info.depth_stencil_format = SdlGpuDepthFormat;
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}

SDL_GPUTexture* CreateTexture(SDL_GPUDevice* device, SdlGpuTargetSize size, SDL_GPUSampleCount sampleCount)
{
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SdlGpuSoftShadow::MaskFormat;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                 (sampleCount == SDL_GPU_SAMPLECOUNT_1 ? SDL_GPU_TEXTUREUSAGE_SAMPLER : 0u);
    info.width = size.width;
    info.height = size.height;
    info.layer_count_or_depth = 1u;
    info.num_levels = 1u;
    info.sample_count = sampleCount;
    return SDL_CreateGPUTexture(device, &info);
}

void ReleaseTexture(SDL_GPUDevice* device, SDL_GPUTexture*& texture)
{
    if (texture)
    {
        SDL_ReleaseGPUTexture(device, texture);
        texture = nullptr;
    }
}

void ReleasePipeline(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline*& pipeline)
{
    if (pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        pipeline = nullptr;
    }
}

SdlGpuTargetSize HalfOf(SdlGpuTargetSize size)
{
    return {std::max(size.width / 2u, 1u), std::max(size.height / 2u, 1u)};
}

SdlGpuBloomUniform MakeUniform(SdlGpuTargetSize sourceSize, float weight)
{
    return {{1.0f / static_cast<float>(sourceSize.width), 1.0f / static_cast<float>(sourceSize.height)}, weight,
            0.0f};
}
} // namespace

bool SdlGpuSoftShadow::CreatePipelines(SDL_GPUDevice* device, SDL_GPUShader* fullscreenVertex,
                                       SDL_GPUShader* blurDown, SDL_GPUShader* blurUp,
                                       SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount)
{
    DestroyPipelines();
    if (!device || !fullscreenVertex || !blurDown || !blurUp)
    {
        return false;
    }
    m_device = device;
    m_invertPipeline =
        BuildFullscreenPipeline(device, fullscreenVertex, blurDown, MaskFormat, SDL_GPU_SAMPLECOUNT_1, Blend::Invert);
    m_downPipeline =
        BuildFullscreenPipeline(device, fullscreenVertex, blurDown, MaskFormat, SDL_GPU_SAMPLECOUNT_1, Blend::None);
    m_upPipeline =
        BuildFullscreenPipeline(device, fullscreenVertex, blurUp, MaskFormat, SDL_GPU_SAMPLECOUNT_1, Blend::None);
    m_compositePipeline =
        BuildFullscreenPipeline(device, fullscreenVertex, blurUp, frameFormat, sampleCount, Blend::Composite);
    if (!m_linearSampler)
    {
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter = SDL_GPU_FILTER_LINEAR;
        sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
        sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_linearSampler = SDL_CreateGPUSampler(device, &sampler);
    }
    if (!IsAvailable())
    {
        DestroyPipelines();
        return false;
    }
    return true;
}

void SdlGpuSoftShadow::DestroyPipelines()
{
    if (!m_device)
    {
        return;
    }
    ReleasePipeline(m_device, m_invertPipeline);
    ReleasePipeline(m_device, m_downPipeline);
    ReleasePipeline(m_device, m_upPipeline);
    ReleasePipeline(m_device, m_compositePipeline);
}

bool SdlGpuSoftShadow::IsAvailable() const
{
    return m_invertPipeline && m_downPipeline && m_upPipeline && m_compositePipeline && m_linearSampler;
}

bool SdlGpuSoftShadow::EnsureTargets(SdlGpuTargetSize size, SDL_GPUSampleCount sampleCount)
{
    if (!IsAvailable() || size.width == 0u || size.height == 0u)
    {
        return false;
    }
    if (m_mask && m_size.width == size.width && m_size.height == size.height && m_sampleCount == sampleCount)
    {
        return true;
    }
    ReleaseTargets();
    m_size = size;
    m_sampleCount = sampleCount;
    m_mask = CreateTexture(m_device, size, sampleCount);
    if (sampleCount != SDL_GPU_SAMPLECOUNT_1)
    {
        m_maskResolve = CreateTexture(m_device, size, SDL_GPU_SAMPLECOUNT_1);
    }
    m_half = CreateTexture(m_device, HalfOf(size), SDL_GPU_SAMPLECOUNT_1);
    m_quarter = CreateTexture(m_device, HalfOf(HalfOf(size)), SDL_GPU_SAMPLECOUNT_1);
    m_halfBlur = CreateTexture(m_device, HalfOf(size), SDL_GPU_SAMPLECOUNT_1);
    if (!m_mask || (sampleCount != SDL_GPU_SAMPLECOUNT_1 && !m_maskResolve) || !m_half || !m_quarter || !m_halfBlur)
    {
        ReleaseTargets();
        return false;
    }
    return true;
}

void SdlGpuSoftShadow::RunPass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUGraphicsPipeline* pipeline,
                               SDL_GPUTexture* target, SdlGpuTargetSize targetSize, SDL_GPUTexture* source,
                               SdlGpuTargetSize sourceSize, bool clearWhite)
{
    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = target;
    colorTarget.clear_color = SDL_FColor{1.0f, 1.0f, 1.0f, 1.0f};
    colorTarget.load_op = clearWhite ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_DONT_CARE;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    colorTarget.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
    if (!pass)
    {
        return;
    }
    const SDL_GPUViewport viewport{
        0.0f, 0.0f, static_cast<float>(targetSize.width), static_cast<float>(targetSize.height), 0.0f, 1.0f};
    const SDL_Rect scissor{0, 0, static_cast<int>(targetSize.width), static_cast<int>(targetSize.height)};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUTextureSamplerBinding binding{source, m_linearSampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    const SdlGpuBloomUniform uniform = MakeUniform(sourceSize, 1.0f);
    SDL_PushGPUFragmentUniformData(commandBuffer, 0, &uniform, sizeof(uniform));
    SDL_DrawGPUPrimitives(pass, FullscreenVertexCount, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

void SdlGpuSoftShadow::Blur(SDL_GPUCommandBuffer* commandBuffer, int levels)
{
    m_result = nullptr;
    if (!IsAvailable() || !m_mask)
    {
        return;
    }
    SDL_GPUTexture* const mask = m_maskResolve ? m_maskResolve : m_mask;
    const SdlGpuTargetSize half = HalfOf(m_size);
    RunPass(commandBuffer, m_invertPipeline, m_half, half, mask, m_size, true);
    m_result = m_half;
    m_resultSize = half;
    if (levels >= 2)
    {
        const SdlGpuTargetSize quarter = HalfOf(half);
        RunPass(commandBuffer, m_downPipeline, m_quarter, quarter, m_half, half, false);
        RunPass(commandBuffer, m_upPipeline, m_halfBlur, half, m_quarter, quarter, false);
        m_result = m_halfBlur;
    }
}

void SdlGpuSoftShadow::DrawComposite(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, float strength)
{
    if (!IsAvailable() || !m_result || !pass)
    {
        return;
    }
    const SDL_GPUViewport viewport{
        0.0f, 0.0f, static_cast<float>(m_size.width), static_cast<float>(m_size.height), 0.0f, 1.0f};
    const SDL_Rect scissor{0, 0, static_cast<int>(m_size.width), static_cast<int>(m_size.height)};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, m_compositePipeline);
    const SDL_GPUTextureSamplerBinding binding{m_result, m_linearSampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    const SdlGpuBloomUniform uniform = MakeUniform(m_resultSize, strength);
    SDL_PushGPUFragmentUniformData(commandBuffer, 0, &uniform, sizeof(uniform));
    SDL_DrawGPUPrimitives(pass, FullscreenVertexCount, 1, 0, 0);
}

void SdlGpuSoftShadow::ReleaseTargets()
{
    if (!m_device)
    {
        return;
    }
    ReleaseTexture(m_device, m_mask);
    ReleaseTexture(m_device, m_maskResolve);
    ReleaseTexture(m_device, m_half);
    ReleaseTexture(m_device, m_quarter);
    ReleaseTexture(m_device, m_halfBlur);
    m_result = nullptr;
    m_size = {};
}

void SdlGpuSoftShadow::Release()
{
    ReleaseTargets();
    DestroyPipelines();
    if (m_device && m_linearSampler)
    {
        SDL_ReleaseGPUSampler(m_device, m_linearSampler);
        m_linearSampler = nullptr;
    }
}

} // namespace mu
