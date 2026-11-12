#include "SdlGpuSsao.h"

#include "Render/PostProcessing/SsaoSetting.h"

#include <algorithm>

namespace mu
{

namespace
{
constexpr Uint32 FullscreenVertexCount = 3u;
// The blur steps one half-size texel at a time.
constexpr float BlurStepTexels = 1.0f;

namespace Ssao = Render::PostProcessing::Ssao;

enum class Blend
{
    None,
    Multiply, // frame * shader colour
};

SDL_GPUGraphicsPipeline* BuildFullscreenPipeline(SDL_GPUDevice* device, SDL_GPUShader* vertexShader,
                                                 SDL_GPUShader* fragmentShader, SDL_GPUTextureFormat format,
                                                 SDL_GPUSampleCount sampleCount, Blend blend)
{
    const bool multiply = blend == Blend::Multiply;
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = format;
    if (multiply)
    {
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
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
    info.multisample_state.sample_count = sampleCount;
    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;
    // The apply pass draws inside the frame's pass, which has the frame's depth; no test, no write.
    info.target_info.has_depth_stencil_target = multiply;
    info.target_info.depth_stencil_format = SdlGpuDepthFormat;
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}

SDL_GPUTexture* CreateTexture(SDL_GPUDevice* device, SdlGpuTargetSize size, SDL_GPUTextureFormat format,
                              SDL_GPUTextureUsageFlags usage)
{
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = size.width;
    info.height = size.height;
    info.layer_count_or_depth = 1u;
    info.num_levels = 1u;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
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

void SetFour(float (&target)[4], float x, float y, float z, float w)
{
    target[0] = x;
    target[1] = y;
    target[2] = z;
    target[3] = w;
}
} // namespace

bool SdlGpuSsao::CreatePipelines(SDL_GPUDevice* device, const SdlGpuSsaoShaders& shaders,
                                 SDL_GPUTextureFormat frameFormat, SDL_GPUSampleCount sampleCount)
{
    DestroyPipelines();
    if (!device || !shaders.fullscreenVertex || !shaders.occlusion || !shaders.blur || !shaders.apply)
    {
        return false;
    }
    m_device = device;
    m_occlusionPipeline = BuildFullscreenPipeline(device, shaders.fullscreenVertex, shaders.occlusion,
                                                  OcclusionFormat, SDL_GPU_SAMPLECOUNT_1, Blend::None);
    m_blurPipeline = BuildFullscreenPipeline(device, shaders.fullscreenVertex, shaders.blur, OcclusionFormat,
                                             SDL_GPU_SAMPLECOUNT_1, Blend::None);
    m_applyPipeline = BuildFullscreenPipeline(device, shaders.fullscreenVertex, shaders.apply, frameFormat,
                                              sampleCount, Blend::Multiply);
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
        DestroyPipelines();
        return false;
    }
    return true;
}

void SdlGpuSsao::DestroyPipelines()
{
    if (!m_device)
    {
        return;
    }
    ReleasePipeline(m_device, m_occlusionPipeline);
    ReleasePipeline(m_device, m_blurPipeline);
    ReleasePipeline(m_device, m_applyPipeline);
    m_resultReady = false;
}

bool SdlGpuSsao::IsAvailable() const
{
    return m_occlusionPipeline && m_blurPipeline && m_applyPipeline && m_pointSampler;
}

bool SdlGpuSsao::EnsureTargets(SdlGpuTargetSize size)
{
    if (!IsAvailable() || m_depthUnsupported || size.width == 0u || size.height == 0u)
    {
        return false;
    }
    if (m_depth && m_size.width == size.width && m_size.height == size.height)
    {
        return true;
    }
    ReleaseTargets();
    constexpr SDL_GPUTextureUsageFlags depthUsage =
        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if (!SDL_GPUTextureSupportsFormat(m_device, SdlGpuDepthFormat, SDL_GPU_TEXTURETYPE_2D, depthUsage))
    {
        m_depthUnsupported = true;
        return false;
    }
    constexpr SDL_GPUTextureUsageFlags occlusionUsage =
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    m_size = size;
    m_depth = CreateTexture(m_device, size, SdlGpuDepthFormat, depthUsage);
    m_occlusion = CreateTexture(m_device, HalfOf(size), OcclusionFormat, occlusionUsage);
    m_blurred = CreateTexture(m_device, HalfOf(size), OcclusionFormat, occlusionUsage);
    if (!m_depth || !m_occlusion || !m_blurred)
    {
        ReleaseTargets();
        return false;
    }
    return true;
}

bool SdlGpuSsao::RunPass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUGraphicsPipeline* pipeline,
                         SDL_GPUTexture* target, SDL_GPUTexture* source)
{
    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = target;
    colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;
    colorTarget.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
    if (!pass)
    {
        return false;
    }
    const SdlGpuTargetSize half = HalfOf(m_size);
    const SDL_GPUViewport viewport{
        0.0f, 0.0f, static_cast<float>(half.width), static_cast<float>(half.height), 0.0f, 1.0f};
    const SDL_Rect scissor{0, 0, static_cast<int>(half.width), static_cast<int>(half.height)};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    const SDL_GPUTextureSamplerBinding binding{source, m_pointSampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_PushGPUFragmentUniformData(commandBuffer, 0, &m_uniform, sizeof(m_uniform));
    SDL_DrawGPUPrimitives(pass, FullscreenVertexCount, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    return true;
}

bool SdlGpuSsao::Render(SDL_GPUCommandBuffer* commandBuffer, const SdlGpuSsaoFrame& frame)
{
    m_resultReady = false;
    if (!IsAvailable() || !m_depth || frame.strength <= 0.0f || frame.viewport.w <= 0.0f ||
        frame.viewport.h <= 0.0f)
    {
        return false;
    }
    const float p00 = frame.projection[0][0];
    const float p11 = frame.projection[1][1];
    const float p22 = frame.projection[2][2];
    const float p32 = frame.projection[3][2];
    if (p00 == 0.0f || p11 == 0.0f)
    {
        return false;
    }
    const float farDistance = Ssao::LinearDepth(1.0f, p22, p32);
    const float width = static_cast<float>(m_size.width);
    const float height = static_cast<float>(m_size.height);
    const SdlGpuTargetSize half = HalfOf(m_size);
    SetFour(m_uniform.projection, 1.0f / p00, 1.0f / p11, p22, p32);
    SetFour(m_uniform.viewport, frame.viewport.x, frame.viewport.y, frame.viewport.w, frame.viewport.h);
    SetFour(m_uniform.fullSize, 1.0f / width, 1.0f / height, width, height);
    SetFour(m_uniform.sampling, Ssao::kRadiusWorld, Ssao::kMaxRadiusPixels, Ssao::kBiasWorld, Ssao::kIntensity);
    SetFour(m_uniform.apply, frame.fadeStart, frame.fadeEnd, std::clamp(frame.strength, 0.0f, 1.0f), Ssao::kFloor);
    SetFour(m_uniform.blur, 0.0f, 0.0f, farDistance, 0.0f);

    if (!RunPass(commandBuffer, m_occlusionPipeline, m_occlusion, m_depth))
    {
        return false;
    }
    m_uniform.blur[0] = BlurStepTexels / static_cast<float>(half.width);
    m_uniform.blur[1] = 0.0f;
    if (!RunPass(commandBuffer, m_blurPipeline, m_blurred, m_occlusion))
    {
        return false;
    }
    m_uniform.blur[0] = 0.0f;
    m_uniform.blur[1] = BlurStepTexels / static_cast<float>(half.height);
    if (!RunPass(commandBuffer, m_blurPipeline, m_occlusion, m_blurred))
    {
        return false;
    }
    m_resultReady = true;
    return true;
}

void SdlGpuSsao::DrawApply(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass)
{
    if (!IsAvailable() || !m_resultReady || !pass)
    {
        return;
    }
    const SDL_GPUViewport viewport{
        0.0f, 0.0f, static_cast<float>(m_size.width), static_cast<float>(m_size.height), 0.0f, 1.0f};
    const SDL_Rect scissor{0, 0, static_cast<int>(m_size.width), static_cast<int>(m_size.height)};
    SDL_SetGPUViewport(pass, &viewport);
    SDL_SetGPUScissor(pass, &scissor);
    SDL_BindGPUGraphicsPipeline(pass, m_applyPipeline);
    // Same order as the shader declares them: the occlusion (t0), the depth (t1).
    const SDL_GPUTextureSamplerBinding bindings[] = {{m_occlusion, m_pointSampler}, {m_depth, m_pointSampler}};
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);
    SDL_PushGPUFragmentUniformData(commandBuffer, 0, &m_uniform, sizeof(m_uniform));
    SDL_DrawGPUPrimitives(pass, FullscreenVertexCount, 1, 0, 0);
}

void SdlGpuSsao::ReleaseTargets()
{
    if (!m_device)
    {
        return;
    }
    ReleaseTexture(m_device, m_depth);
    ReleaseTexture(m_device, m_occlusion);
    ReleaseTexture(m_device, m_blurred);
    m_resultReady = false;
    m_size = {};
}

void SdlGpuSsao::Release()
{
    ReleaseTargets();
    DestroyPipelines();
    if (m_device && m_pointSampler)
    {
        SDL_ReleaseGPUSampler(m_device, m_pointSampler);
        m_pointSampler = nullptr;
    }
}

} // namespace mu
