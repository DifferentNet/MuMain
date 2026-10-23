// Web build only. RmlUi's SDL GPU renderer (ThirdParty/RmlUi/Backends/RmlUi_Renderer_SDL_GPU.cpp)
// carries its shaders as SPIR-V, MSL and DXIL, but the WebGPU backend takes WGSL. Rather than
// changing the RmlUi submodule, the web build compiles that one file with SDL_GetGPUShaderFormats
// and SDL_CreateGPUShader renamed to the two functions below (src/CMakeLists.txt): RmlUi picks its
// SPIR-V, and the SPIR-V is swapped for the matching WGSL from RmlUiShadersWgsl.h, which
// tools/web/rmlui-shaders-to-wgsl.mjs generates.

#include "Core/Platform/web/RmlUiShadersWgsl.h"

#include <SDL3/SDL_gpu.h>

#include <cstring>

namespace
{
std::uint64_t Fnv1a64(const Uint8* data, std::size_t size)
{
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < size; ++i)
    {
        hash = (hash ^ data[i]) * 0x100000001b3ull;
    }
    return hash;
}

bool TakesWgsl(SDL_GPUDevice* device)
{
    return (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_WGSL) != 0;
}
} // namespace

extern "C" SDL_GPUShaderFormat MuWeb_RmlUi_GetGPUShaderFormats(SDL_GPUDevice* device)
{
    // RmlUi uses the first format it knows; its SPIR-V is what gets matched below.
    return TakesWgsl(device) ? SDL_GPU_SHADERFORMAT_SPIRV : SDL_GetGPUShaderFormats(device);
}

extern "C" SDL_GPUShader* MuWeb_RmlUi_CreateGPUShader(SDL_GPUDevice* device, const SDL_GPUShaderCreateInfo* info)
{
    if (info->format != SDL_GPU_SHADERFORMAT_SPIRV || !TakesWgsl(device))
    {
        return SDL_CreateGPUShader(device, info);
    }

    const std::uint64_t hash = Fnv1a64(info->code, info->code_size);
    for (const mu::web::RmlUiWgslShader& shader : mu::web::RmlUiWgslShaders)
    {
        if (shader.spirvHash == hash && shader.spirvSize == info->code_size)
        {
            SDL_GPUShaderCreateInfo wgsl = *info;
            wgsl.code = reinterpret_cast<const Uint8*>(shader.wgsl);
            wgsl.code_size = std::strlen(shader.wgsl); // the literal's NUL ends it; the backend reads it as a string too
            wgsl.format = SDL_GPU_SHADERFORMAT_WGSL;
            wgsl.entrypoint = "main";
            return SDL_CreateGPUShader(device, &wgsl);
        }
    }

    SDL_SetError("No WGSL version of this RmlUi shader: run tools/web/rmlui-shaders-to-wgsl.mjs");
    return nullptr;
}
