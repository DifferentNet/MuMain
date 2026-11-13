// Bitmap decoding, upload, registration and unload share the production bitmap state.
#include "stdafx.h"
#include "turbojpeg.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Render/Renderer/BitmapUpload.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Sprites/BitmapTextureLimits.h"
#include "GlobalBitmapPath.h"

#include "Render/RmlUi/RmlPremultiply.h"
#include "Render/Textures/FoliageSmoothing.h"
#include "Render/Textures/MipChain.h"
#include "Render/Textures/OztFile.h"
#include "Render/Textures/PackedOzt.h"
#include "Render/Textures/TextureFiltering.h"
#include "Render/Textures/HdTextureSize.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/NormalMapFile.h"
#include "Render/Textures/NormalMaps.h"
#include "Render/Textures/UpscaleMarker.h"
#ifdef _EDITOR
#include "Core/Utilities/AssetLoadWorld.h"
#endif

#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace mu
{
void RegisterTexture(std::uint32_t id, void* texture);
void UnregisterTexture(std::uint32_t id);
void RegisterSampler(std::uint32_t id, void* sampler);
void UnregisterSampler(std::uint32_t id);
void ClearTextureRegistry();
void ClearSamplerRegistry();
void SetTextureMissHandler(bool (*handler)(std::uint32_t id));
void RegisterFilteredSampler(std::uint32_t id, void* sampler);
void MarkFilteredSamplerPending(std::uint32_t id);
void TakeFilteredSamplerRequests(std::vector<std::uint32_t>& requests);
void SetTextureFilteringEnabled(bool enabled);
void RegisterNormalMap(std::uint32_t id, SDL_GPUTexture* texture);
void UnregisterNormalMap(std::uint32_t id);
} // namespace mu

namespace
{
class TurboJpegHandle
{
public:
    TurboJpegHandle() : handle(tjInitDecompress()) {}
    ~TurboJpegHandle()
    {
        if (handle != nullptr)
        {
            tjDestroy(handle);
        }
    }

    tjhandle get() const
    {
        return handle;
    }
    bool valid() const
    {
        return handle != nullptr;
    }

private:
    tjhandle handle;
};

void ReportTurboError(const wchar_t* context)
{
    const char* message = tjGetErrorStr();
    if (message == nullptr)
    {
        message = "Unknown TurboJPEG error";
    }
    g_ErrorReport.Write(L"[TurboJPEG] %ls: %hs", context, message);
}

int NextPowerOfTwo(int value, int maxValue)
{
    int result = 1;
    while (result < value && result < maxValue)
    {
        result <<= 1;
    }
    return std::min<int>(result, maxValue);
}

std::unique_ptr<BITMAP_t> CreateBitmapStorage(GLuint index, const std::wstring& filename, int width, int height,
                                              int components)
{
    auto bitmap = std::make_unique<BITMAP_t>();
    bitmap->BitmapIndex = index;
    wcsncpy(bitmap->FileName, filename.c_str(), MAX_BITMAP_FILE_NAME - 1);
    bitmap->FileName[MAX_BITMAP_FILE_NAME - 1] = L'\0';
    bitmap->Width = static_cast<float>(width);
    bitmap->Height = static_cast<float>(height);
    bitmap->Components = components;
    bitmap->Ref = 1;
    const auto bufferSize =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(components);
    bitmap->BufferStorage.resize(bufferSize);
    bitmap->Buffer = bitmap->BufferStorage.data();
    return bitmap;
}

SDL_GPUFilter MapGLFilterToSDL(GLuint filter)
{
    return filter == GL_NEAREST ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
}

SDL_GPUSamplerAddressMode MapGLWrapToSDL(GLuint wrapMode)
{
    return wrapMode == GL_REPEAT ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
}

std::vector<std::uint8_t> PadRGBToRGBA(const BYTE* rgbData, int width, int height)
{
    const auto pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> rgba(pixelCount * 4u);
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        rgba[i * 4u + 0u] = rgbData[i * 3u + 0u];
        rgba[i * 4u + 1u] = rgbData[i * 3u + 1u];
        rgba[i * 4u + 2u] = rgbData[i * 3u + 2u];
        rgba[i * 4u + 3u] = 255u;
    }
    return rgba;
}

// Nothing reads the CPU copy of a texture after the upload except for the
// canvases (LoadCanvasBitmap), which are redrawn through Buffer. The copy of an
// upscaled texture is up to 16 times the original; keeping the copies took the
// 32 bit client past 3.3 GB of address space.
void ReleaseUploadedPixels(BITMAP_t& bitmap)
{
    std::vector<std::uint8_t>().swap(bitmap.BufferStorage);
    bitmap.Buffer = nullptr;
}

// The size of the base level of an uploaded texture, which the loader counts as
// texture memory.
std::uint32_t UploadedBytes(int width, int height)
{
    return static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) * 4u;
}

// The ground's tile slots (BITMAP_MAPTILE + n): drawn as terrain, also when a tile has alpha (Hellas
// water1, Karutan AlphaTile01), so never a foliage cut-out.
bool IsTerrainTileIndex(GLuint index)
{
    return index >= BITMAP_MAPTILE_BEGIN && index <= BITMAP_MAPTILE_END;
}

// Whether RGBA pixels loaded from `filename` into `index` are a map's own cut-out
// (Render/Textures/FoliageSmoothing.h). Only the Remaster PC client uses it, so only it pays for the
// alpha pass.
bool IsMapCutout(GLuint index, const std::wstring& filename, std::span<const std::uint8_t> rgba, int width,
                 int height)
{
#ifdef __EMSCRIPTEN__
    (void)index;
    (void)filename;
    (void)rgba;
    (void)width;
    (void)height;
    return false;
#else
    if (!Render::Textures::Hd::Enabled() || IsTerrainTileIndex(index) ||
        !Render::Textures::IsMapCutoutCandidatePath(filename) || width <= 0 || height <= 0 ||
        rgba.size() < UploadedBytes(width, height))
    {
        return false;
    }
    const float coverage = Render::Textures::AlphaCoverage(
        rgba, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
        Render::Textures::AlphaTest::FromReference(Render::Textures::AlphaTestReference));
    return Render::Textures::FoliageSmoothing::IsCutout(coverage);
#endif
}

// A canvas is drawn on through Buffer at the original size, so an upscaled file
// loaded into one is brought back to that size. Width and height become the
// size to upload.
void ShrinkCanvas(BITMAP_t& bitmap, int& width, int& height, int scale)
{
    bitmap.BufferStorage =
        Render::Textures::Upscale::ShrinkToOriginal(bitmap.BufferStorage, width, height, bitmap.Components, scale);
    bitmap.Buffer = bitmap.BufferStorage.data();
    width /= scale;
    height /= scale;
}

// The levels below the image: built on the CPU (`chain`) or by the GPU (`generate`).
struct MipLevels
{
    std::vector<Render::Textures::MipLevel> chain;
    bool generate = false;
};

bool UploadTextureSDLGpu(BITMAP_t* bitmap, std::span<const std::uint8_t> pixels, int width, int height,
                         SDL_GPUFilter filter, SDL_GPUSamplerAddressMode wrapMode, const MipLevels& mips)
{
    auto result = mu::UploadBitmapPixels(pixels, width, height, filter, wrapMode, mips.chain, mips.generate);
    if (!result)
    {
        const char* driver = mu::BitmapUploadDriverName();
        g_ErrorReport.Write(
            L"SDL bitmap upload failed at %hs: %hs (id=%u, asset=%ls, %dx%d, bytes=%zu, driver=%hs)\r\n",
            result.stage.c_str(), result.error.c_str(), bitmap->BitmapIndex, bitmap->FileName, width, height,
            pixels.size(), driver ? driver : "unknown");
        return false;
    }
    bitmap->sdlTexture = result.texture;
    bitmap->sdlSampler = result.sampler;
    bitmap->mipLevels = static_cast<std::uint8_t>(
        mips.generate ? Render::Textures::CountMipLevels(static_cast<std::uint32_t>(width),
                                                         static_cast<std::uint32_t>(height))
                      : mips.chain.size() + 1u);
    return true;
}

// The Remaster's normal map of an upscaled model texture, read from its prebuilt file next to the
// texture (Render/Textures/NormalMapFile.h) and mipmapped by the GPU like the texture. A missing or
// broken file only costs the relief; MU_NORMAL_MAPS_RUNTIME=1 builds it here instead (development).
// `width` x `height` is the texture as uploaded and `scale` its scale: the file held `fileDivisor`
// times that (Render/Textures/HdTextureSize.h), and its map is brought down by as much.
void UploadNormalMap(BITMAP_t& bitmap, const std::wstring& loadedPath, std::span<const std::uint8_t> rgba, int width,
                     int height, int scale, int fileDivisor, SDL_GPUSamplerAddressMode wrapMode)
{
    namespace NormalMaps = Render::Textures::NormalMaps;
    namespace NormalMapFile = Render::Textures::NormalMapFile;
    if (!NormalMaps::Enabled() || !NormalMaps::WantsNormalMap(loadedPath))
    {
        return;
    }
    const int fileWidth = width * fileDivisor;
    const int fileHeight = height * fileDivisor;
    int mapWidth = NormalMapFile::HalfSize(fileWidth);
    int mapHeight = NormalMapFile::HalfSize(fileHeight);
    std::vector<std::uint8_t> normals = NormalMapFile::Load(
        loadedPath, fileWidth, fileHeight,
        [](const std::wstring& path, std::size_t maxBytes, std::vector<std::uint8_t>& bytes) {
            std::ifstream input(Render::Sprites::NarrowBitmapPath(path), std::ios::binary | std::ios::ate);
            if (!input)
            {
                return false;
            }
            const std::streamoff size = input.tellg();
            if (size < 0 || static_cast<std::uint64_t>(size) > maxBytes)
            {
                return false;
            }
            try
            {
                bytes.resize(static_cast<std::size_t>(size));
            }
            catch (const std::bad_alloc&)
            {
                return false;
            }
            input.seekg(0);
            return static_cast<bool>(
                input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())));
        });
    if (!normals.empty() && fileDivisor == 2)
    {
        // Tilts average like colours; a map with an odd side stays as its file has it.
        std::vector<std::uint8_t> half = Render::Textures::HdSize::HalveRgba(normals, mapWidth, mapHeight, false);
        if (!half.empty())
        {
            normals = std::move(half);
            mapWidth /= 2;
            mapHeight /= 2;
        }
    }
    if (normals.empty() && NormalMaps::RuntimeBuild())
    {
        normals = NormalMaps::Build(rgba, width, height, scale);
        mapWidth = width;
        mapHeight = height;
    }
    if (normals.empty())
    {
        return;
    }
    auto result = mu::UploadBitmapPixels(normals, mapWidth, mapHeight, SDL_GPU_FILTER_LINEAR, wrapMode, {}, true);
    if (!result)
    {
        mu::log::Get("render")->debug("Normal map upload failed for {}: {}", mu_wchar_to_utf8(loadedPath.c_str()),
                                      result.error);
        return;
    }
    bitmap.sdlNormalMap = result.texture;
    // Counted with its texture, so the texture memory the game reports includes the relief.
    bitmap.UploadedBytes += UploadedBytes(mapWidth, mapHeight);
}

void ReleaseNormalMap(SDL_GPUDevice* device, BITMAP_t& bitmap)
{
    mu::UnregisterNormalMap(bitmap.BitmapIndex);
    if (device && bitmap.sdlNormalMap)
    {
        mu::ReleaseBitmapTexture(device, bitmap.sdlNormalMap);
    }
    bitmap.sdlNormalMap = nullptr;
}

// Enhanced samplers cover the whole chain (Vulkan's VK_LOD_CLAMP_NONE).
constexpr float UnclampedMaxLod = 1000.0f;

// CGlobalBitmap::m_filteredSamplers: the cut-out pair (clamp, repeat) starts here.
constexpr std::size_t kCutoutSamplerFirst = 2u;

// Build mip chains requested by world draws for at most this long per frame.
constexpr std::chrono::milliseconds MipChainBudgetPerFrame{4};

// The mip chain of a bitmap's RGBA8 pixels. Textures with an alpha channel
// keep their alpha-test coverage at every level (box filtered); opaque ones use `opaqueFilter`.
std::vector<Render::Textures::MipLevel> BuildBitmapMipChain(
    const std::uint8_t* rgba, int width, int height, bool hasAlpha, double& seconds,
    Render::Textures::MipFilter opaqueFilter = Render::Textures::MipFilter::Box)
{
    const auto start = std::chrono::steady_clock::now();
    const std::span<const std::uint8_t> pixels(rgba, static_cast<std::size_t>(width) * height * 4u);
    std::vector<Render::Textures::MipLevel> chain =
        hasAlpha ? Render::Textures::BuildCoveragePreservingMipChain(
                       pixels, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
                       Render::Textures::AlphaTest::FromReference(Render::Textures::AlphaTestReference))
                 : Render::Textures::BuildMipChain(pixels, static_cast<std::uint32_t>(width),
                                                   static_cast<std::uint32_t>(height), opaqueFilter);
    seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return chain;
}

// Upscaled art, and world textures while texture filtering is on, get every mip
// level. The GPU builds them for an opaque image, box filtered as the CPU would;
// an image with alpha keeps its alpha-test coverage at every level on the CPU.
// `sharp` (opaque upscaled art) builds the opaque chain on the CPU with the sharper
// filter instead: the box-filtered HD levels made the Remaster's ground look soft at a
// distance next to Classic (ghinzuro 2026-11-12).
MipLevels PlanMipLevels(bool mipmapped, const std::uint8_t* rgba, int width, int height, bool hasAlpha,
                        double& seconds, bool sharp = false)
{
    const Render::Textures::MipFilter opaqueFilter =
        sharp ? Render::Textures::MipFilter::Sharp : Render::Textures::MipFilter::Box;
    if (!mipmapped)
    {
        return {};
    }
#ifdef __EMSCRIPTEN__
    // Browser build: the CPU builds every chain. The WebGPU backend's GPU mipmap generation
    // corrupted memory when Enhanced graphics switched on (2026-10-22).
    return {BuildBitmapMipChain(rgba, width, height, hasAlpha, seconds, opaqueFilter), false};
#else
    if (!hasAlpha)
    {
        if (sharp)
        {
            return {BuildBitmapMipChain(rgba, width, height, false, seconds, opaqueFilter), false};
        }
        return {{}, true};
    }
    return {BuildBitmapMipChain(rgba, width, height, true, seconds), false};
#endif
}

// GPU bytes of a bitmap's RGBA8 texture, all of its mip levels included.

std::uint64_t GpuTextureBytes(const BITMAP_t& bitmap)
{
    auto width = static_cast<std::uint32_t>(bitmap.Width) * bitmap.scale;
    auto height = static_cast<std::uint32_t>(bitmap.Height) * bitmap.scale;
    std::uint64_t bytes = 0;
    for (std::uint32_t level = 0; level < bitmap.mipLevels; ++level)
    {
        bytes += static_cast<std::uint64_t>(width) * height * 4u;
        width = std::max<std::uint32_t>(1u, width / 2u);
        height = std::max<std::uint32_t>(1u, height / 2u);
    }
    return bytes;
}
} // namespace

GLuint CGlobalBitmap::LoadImage(const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode)
{
    BITMAP_t* pBitmap = FindTextureEntry(filename);
    if (pBitmap)
    {
        if (pBitmap->Ref > 0)
        {
            if (0 == _wcsicmp(pBitmap->FileName, filename.c_str()))
            {
                pBitmap->Ref++;

                return pBitmap->BitmapIndex;
            }
        }
    }
    else
    {
        GLuint uiNewTextureIndex = GenerateTextureIndex();
        if (true == LoadImage(uiNewTextureIndex, filename, uiFilter, uiWrapMode))
        {
            m_listNonamedIndex.push_back(uiNewTextureIndex);

            return uiNewTextureIndex;
        }
    }
    return BITMAP_UNKNOWN;
}
GLuint CGlobalBitmap::LoadImageExclusive(const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode,
                                         bool padToPowerOfTwo, bool premultiplyAlpha)
{
    GLuint uiNewTextureIndex = GenerateTextureIndex();
    if (true == LoadImage(uiNewTextureIndex, filename, uiFilter, uiWrapMode, padToPowerOfTwo, premultiplyAlpha))
    {
        m_listNonamedIndex.push_back(uiNewTextureIndex);
        return uiNewTextureIndex;
    }
    return BITMAP_UNKNOWN;
}
GLuint CGlobalBitmap::LoadChangedImage(const std::wstring& filename, const std::wstring& name, GLuint uiFilter,
                                       GLuint uiWrapMode, const PixelChange& change)
{
    if (BITMAP_t* known = FindTextureEntry(name); known != nullptr && known->Ref > 0)
    {
        return known->BitmapIndex;
    }

    std::wstring ext;
    SplitExt(filename, ext, false);
    const bool jpg = 0 == _wcsicmp(ext.c_str(), L"jpg");
    if (!jpg && 0 != _wcsicmp(ext.c_str(), L"tga"))
    {
        return BITMAP_UNKNOWN;
    }

    const GLuint index = GenerateTextureIndex();
    m_pixelChange = &change;
    const bool loaded = jpg ? OpenJpegTurbo(index, filename, uiFilter, uiWrapMode, false)
                            : OpenTga(index, filename, uiFilter, uiWrapMode, false);
    m_pixelChange = nullptr;
    const auto added = m_mapBitmap.find(index);
    if (!loaded || added == m_mapBitmap.end())
    {
        return BITMAP_UNKNOWN;
    }

    BITMAP_t& copy = *added->second;
    wcsncpy(copy.FileName, name.c_str(), MAX_BITMAP_FILE_NAME - 1);
    copy.FileName[MAX_BITMAP_FILE_NAME - 1] = L'\0';
    m_listNonamedIndex.push_back(index);
    m_BitmapCache.Remove(index);
    return index;
}

bool CGlobalBitmap::LoadImage(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode,
                              bool padToPowerOfTwo, bool premultiplyAlpha, bool keepPixels)
{
    unsigned int UICLAMP = GL_CLAMP_TO_EDGE;
    unsigned int UIREPEAT = GL_REPEAT;

    if (uiWrapMode != UICLAMP && uiWrapMode != UIREPEAT)
    {
#ifdef _DEBUG
        static unsigned int uiCnt2 = 0;

        wchar_t szDebugOutput[256];

        mu_swprintf_s(szDebugOutput, L"%d. Call No CLAMP & No REPEAT. \n", uiCnt2++);
        OutputDebugString(szDebugOutput);
#endif
    }

    auto mi = m_mapBitmap.find(uiBitmapIndex);
    // A canvas keeps its pixels, which a recorded file loads without: the record makes way for a
    // full load below, which keeps its count.
    std::uint8_t recordedRef = 0;
    if (mi != m_mapBitmap.end() && keepPixels && mi->second->pendingLoad &&
        0 == _wcsicmp(mi->second->FileName, filename.c_str()))
    {
        recordedRef = mi->second->Ref;
        m_mapBitmap.erase(mi);
        --m_pendingLoads;
        m_BitmapCache.Remove(uiBitmapIndex);
        mi = m_mapBitmap.end();
    }
    if (mi != m_mapBitmap.end())
    {
        BITMAP_t* pBitmap = mi->second.get();
        if (pBitmap->Ref > 0)
        {
            if (0 == _wcsicmp(pBitmap->FileName, filename.c_str()))
            {
                pBitmap->Ref++;
#ifdef _EDITOR
                // The last loader, as for models: a map that loads the file
                // again has loaded it too.
                m_loadWorlds[uiBitmapIndex] = Core::AssetLoadWorld::Get();
#endif
                return true;
            }
            else
            {
                mu::log::Get("render")->debug("SDL_gpu -- bitmap id {} replaced: {} -> {}", uiBitmapIndex,
                                              mu_wchar_to_utf8(pBitmap->FileName), mu_wchar_to_utf8(filename.c_str()));
                UnloadImage(uiBitmapIndex, true);
            }
        }
    }

    std::wstring ext;
    SplitExt(filename, ext, false);

    const bool worldTexture = Render::Textures::IsWorldTexturePath(filename);
    const bool jpg = 0 == _wcsicmp(ext.c_str(), L"jpg");
    if (m_deferLoads && !keepPixels && (jpg || 0 == _wcsicmp(ext.c_str(), L"tga")))
    {
        // Only a file that is there: the caller tries the next folder when this one fails. A file
        // outside the startup pack is a link to the server copy; its link is enough.
        std::wstring packed;
        ExchangeExt(filename, jpg ? L"OZJ" : L"OZT", packed);
        // A texture of the HD model pack has only its HD copy.
        packed = Render::Textures::Hd::Resolve(packed);
        std::error_code error;
        const auto status = std::filesystem::symlink_status(Render::Sprites::NarrowBitmapPath(packed), error);
        if (!error && std::filesystem::exists(status))
        {
            auto pending = std::make_unique<BITMAP_t>();
            pending->BitmapIndex = uiBitmapIndex;
            wcsncpy(pending->FileName, filename.c_str(), MAX_BITMAP_FILE_NAME - 1);
            pending->FileName[MAX_BITMAP_FILE_NAME - 1] = L'\0';
            pending->Ref = 1;
            pending->TextureNumber = uiBitmapIndex;
            pending->worldTexture = worldTexture;
            pending->wrapRepeat = uiWrapMode == GL_REPEAT;
            pending->pendingLoad = true;
            pending->pendingPad = padToPowerOfTwo;
            pending->pendingPremultiply = premultiplyAlpha;
            pending->pendingFilter = uiFilter;
            pending->pendingWrap = uiWrapMode;
            if (m_mapBitmap.insert(type_bitmap_map::value_type(uiBitmapIndex, std::move(pending))).second)
            {
                ++m_pendingLoads;
            }
            m_BitmapCache.Remove(uiBitmapIndex);
#ifdef _EDITOR
            m_loadWorlds[uiBitmapIndex] = Core::AssetLoadWorld::Get();
#endif
            return true;
        }
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    bool loaded = false;
    if (0 == _wcsicmp(ext.c_str(), L"jpg"))
        loaded =
            OpenJpegTurbo(uiBitmapIndex, filename, uiFilter, uiWrapMode, worldTexture, padToPowerOfTwo, keepPixels);
    else if (0 == _wcsicmp(ext.c_str(), L"tga"))
        loaded = OpenTga(uiBitmapIndex, filename, uiFilter, uiWrapMode, worldTexture, padToPowerOfTwo,
                         premultiplyAlpha, keepPixels);
    m_textureLoadSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (loaded && recordedRef > 0)
    {
        if (const auto added = m_mapBitmap.find(uiBitmapIndex); added != m_mapBitmap.end())
        {
            added->second->Ref = static_cast<std::uint8_t>(added->second->Ref + recordedRef);
        }
    }
#ifdef _EDITOR
    if (loaded)
        m_loadWorlds[uiBitmapIndex] = Core::AssetLoadWorld::Get();
#endif
    return loaded;
}

namespace
{
CGlobalBitmap* s_deferredLoadOwner = nullptr;

bool ResolvePendingBitmap(std::uint32_t id)
{
    return s_deferredLoadOwner != nullptr && s_deferredLoadOwner->ResolvePendingLoad(id);
}
} // namespace

void CGlobalBitmap::BeginDeferredLoads()
{
    m_deferLoads = true;
    s_deferredLoadOwner = this;
    // A draw with a number the renderer doesn't know loads the recorded file.
    mu::SetTextureMissHandler(&ResolvePendingBitmap);
}

void CGlobalBitmap::EndDeferredLoads()
{
    m_deferLoads = false;
}

bool CGlobalBitmap::ResolvePendingLoad(GLuint uiBitmapIndex)
{
    auto mi = m_mapBitmap.find(uiBitmapIndex);
    if (mi == m_mapBitmap.end() || !mi->second->pendingLoad)
    {
        return false;
    }

    const BitmapPtr pending = std::move(mi->second);
    m_mapBitmap.erase(mi);
    --m_pendingLoads;
    m_BitmapCache.Remove(uiBitmapIndex);

    const std::wstring filename = pending->FileName;
    std::wstring ext;
    SplitExt(filename, ext, false);
    const auto start = std::chrono::steady_clock::now();
    bool loaded = false;
    if (0 == _wcsicmp(ext.c_str(), L"jpg"))
        loaded = OpenJpegTurbo(uiBitmapIndex, filename, pending->pendingFilter, pending->pendingWrap,
                               pending->worldTexture, pending->pendingPad, false);
    else
        loaded = OpenTga(uiBitmapIndex, filename, pending->pendingFilter, pending->pendingWrap, pending->worldTexture,
                         pending->pendingPad, pending->pendingPremultiply, false);
    m_textureLoadSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    mi = m_mapBitmap.find(uiBitmapIndex);
    if (!loaded || mi == m_mapBitmap.end())
    {
        mu::log::Get("render")->warn("Deferred texture {} failed to load: {}", uiBitmapIndex,
                                     mu_wchar_to_utf8(filename.c_str()));
        return false;
    }
    BITMAP_t& bitmap = *mi->second;
    bitmap.Ref = pending->Ref;
    bitmap.IsSkin = pending->IsSkin;
    bitmap.IsHair = pending->IsHair;
    m_BitmapCache.Remove(uiBitmapIndex);
    return true;
}

void CGlobalBitmap::UnloadImage(GLuint uiBitmapIndex, bool bForce)
{
    auto mi = m_mapBitmap.find(uiBitmapIndex);
    if (mi != m_mapBitmap.end())
    {
        BITMAP_t* pBitmap = mi->second.get();

        if (pBitmap->pendingLoad)
        {
            // Nothing is on the GPU, and the renderer never knew the number.
            if (--pBitmap->Ref == 0 || bForce)
            {
                m_mapBitmap.erase(mi);
                --m_pendingLoads;
#ifdef _EDITOR
                m_loadWorlds.erase(uiBitmapIndex);
#endif
                if (uiBitmapIndex >= BITMAP_NONAMED_TEXTURES_BEGIN && uiBitmapIndex <= BITMAP_NONAMED_TEXTURES_END)
                {
                    m_listNonamedIndex.remove(uiBitmapIndex);
                }
                m_BitmapCache.Remove(uiBitmapIndex);
            }
            return;
        }

        if (--pBitmap->Ref == 0 || bForce)
        {
            mu::UnregisterTexture(uiBitmapIndex);
            mu::UnregisterSampler(uiBitmapIndex);
            SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(mu::GetRenderer().GetDevice());
            ReleaseNormalMap(device, *pBitmap);
            if (device)
            {
                pBitmap->sdlSampler = nullptr;
                if (pBitmap->sdlTexture)
                {
                    mu::ReleaseBitmapTexture(device, pBitmap->sdlTexture);
                    pBitmap->sdlTexture = nullptr;
                }
            }

            m_dwUsedTextureMemory -= pBitmap->UploadedBytes;

            m_mapBitmap.erase(mi);
#ifdef _EDITOR
            m_loadWorlds.erase(uiBitmapIndex);
#endif

            if (uiBitmapIndex >= BITMAP_NONAMED_TEXTURES_BEGIN && uiBitmapIndex <= BITMAP_NONAMED_TEXTURES_END)
            {
                m_listNonamedIndex.remove(uiBitmapIndex);
            }
            m_BitmapCache.Remove(uiBitmapIndex);
        }
    }
}
#ifdef _EDITOR
std::optional<int> CGlobalBitmap::GetLoadWorld(GLuint uiBitmapIndex) const
{
    const auto found = m_loadWorlds.find(uiBitmapIndex);
    if (found == m_loadWorlds.end())
        return std::nullopt;
    return found->second;
}
#endif

void CGlobalBitmap::UnloadAllImages()
{
    if (m_mapBitmap.empty())
    {
        // The bitmaps may have been unloaded one by one; the shared Enhanced
        // samplers outlive them.
        ReleaseFilteredSamplers();
        m_mipChainRequests.clear();
        return;
    }

#ifdef _DEBUG
    if (!m_mapBitmap.empty())
        g_ErrorReport.Write(L"Unload Images\r\n");
#endif // _DEBUG

    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(mu::GetRenderer().GetDevice());
    if (device)
    {
        for (auto& pair : m_mapBitmap)
        {
            BITMAP_t* pBitmap = pair.second.get();
            if (pBitmap->pendingLoad)
            {
                continue;
            }
            mu::UnregisterTexture(pBitmap->BitmapIndex);
            mu::UnregisterSampler(pBitmap->BitmapIndex);
            ReleaseNormalMap(device, *pBitmap);
            pBitmap->sdlSampler = nullptr;
            if (pBitmap->sdlTexture)
            {
                mu::ReleaseBitmapTexture(device, pBitmap->sdlTexture);
                pBitmap->sdlTexture = nullptr;
            }
        }
    }
    else
    {
        mu::ClearTextureRegistry();
        mu::ClearSamplerRegistry();
    }
    ReleaseFilteredSamplers();
    m_mipChainRequests.clear();

    for ([[maybe_unused]] auto& pair : m_mapBitmap)
    {
#ifdef _DEBUG
        BITMAP_t* pBitmap = pair.second.get();
        if (pBitmap->Ref > 1)
        {
            g_ErrorReport.Write(L"Bitmap %ls(RefCount= %d)\r\n", pBitmap->FileName, pBitmap->Ref);
        }
#endif // _DEBUG
    }

    m_mapBitmap.clear();
    m_pendingLoads = 0;
#ifdef _EDITOR
    m_loadWorlds.clear();
#endif
    m_listNonamedIndex.clear();
    m_BitmapCache.RemoveAll();

    Init();
}

GLuint CGlobalBitmap::GenerateTextureIndex()
{
    GLuint uiAvailableTextureIndex = FindAvailableTextureIndex(m_uiTextureIndexStream);
    if (uiAvailableTextureIndex >= BITMAP_NONAMED_TEXTURES_END)
    {
        m_uiAlternate++;
        m_uiTextureIndexStream = BITMAP_NONAMED_TEXTURES_BEGIN;
        uiAvailableTextureIndex = FindAvailableTextureIndex(m_uiTextureIndexStream);
    }
    return m_uiTextureIndexStream = uiAvailableTextureIndex;
}
GLuint CGlobalBitmap::FindAvailableTextureIndex(GLuint uiSeed)
{
    if (m_uiAlternate > 0)
    {
        auto li = std::find(m_listNonamedIndex.begin(), m_listNonamedIndex.end(), uiSeed + 1);
        if (li != m_listNonamedIndex.end())
            return FindAvailableTextureIndex(uiSeed + 1);
    }
    return uiSeed + 1;
}

bool CGlobalBitmap::OpenJpegTurbo(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter,
                                  GLuint uiWrapMode, bool worldTexture, bool padToPowerOfTwo, bool keepPixels)
{
    std::wstring filename_ozj;
    ExchangeExt(filename, L"OZJ", filename_ozj);

    // The HD copy when the player has HD textures on (Render/Textures/HdTextures.h).
    // A texture whose pixels the game reads keeps the original.
    if (!keepPixels)
    {
        filename_ozj = Render::Textures::Hd::Resolve(filename_ozj);
    }
    std::ifstream compressedFile(Render::Sprites::NarrowBitmapPath(filename_ozj), std::ios::binary);
    if (!compressedFile)
    {
        mu::log::Get("render")->debug("OpenJpegTurbo -- file open failed: {}", mu_wchar_to_utf8(filename_ozj.c_str()));
        return false;
    }

    std::vector<unsigned char> jpegBuf((std::istreambuf_iterator<char>(compressedFile)),
                                       std::istreambuf_iterator<char>());
    compressedFile.close();

    if (jpegBuf.size() <= 24)
    {
        g_ErrorReport.Write(L"OpenJpegTurbo: file too small %ls (%zu bytes)\r\n", filename_ozj.c_str(), jpegBuf.size());
        return false;
    }

    // A marked file holds an image up to its scale times the largest original.
    const int markedScale = Render::Textures::Upscale::ReadMarkedScale(jpegBuf);
    const int maxWidth = Render::Sprites::kBitmapMaximumWidth * markedScale;
    const int maxHeight = Render::Sprites::kBitmapMaximumHeight * markedScale;

    // Skip first 24 bytes (OZJ header)
    const unsigned char* jpegData = jpegBuf.data() + 24;
    const auto jpegSize = static_cast<unsigned long>(jpegBuf.size() - 24);

    int jpegWidth = 0, jpegHeight = 0;
    int jpegSubsamp = TJSAMP_444;
    int jpegColorspace = TJCS_RGB;

    TurboJpegHandle tjHandle;
    if (!tjHandle.valid())
    {
        ReportTurboError(L"tjInitDecompress");
        return false;
    }

    auto headerResult =
        tjDecompressHeader3(tjHandle.get(), jpegData, jpegSize, &jpegWidth, &jpegHeight, &jpegSubsamp, &jpegColorspace);
    if (headerResult != 0 || jpegWidth <= 0 || jpegHeight <= 0)
    {
        ReportTurboError(L"tjDecompressHeader3");
        return false;
    }
    if (jpegWidth > maxWidth || jpegHeight > maxHeight)
    {
        g_ErrorReport.Write(L"OpenJpegTurbo: image too large %ls (%dx%d, at most %dx%d)\r\n", filename_ozj.c_str(),
                            jpegWidth, jpegHeight, maxWidth, maxHeight);
        return false;
    }

    // The scale is read against the image as the file holds it. With HD textures at half size
    // (Render/Textures/HdTextureSize.h) an HD copy is decoded at half its sides right here, by the
    // JPEG decoder's own scaling: less work than decoding it whole and shrinking it.
    const int fileTextureWidth = padToPowerOfTwo ? NextPowerOfTwo(jpegWidth, maxWidth) : jpegWidth;
    const int fileTextureHeight = padToPowerOfTwo ? NextPowerOfTwo(jpegHeight, maxHeight) : jpegHeight;
    const int fileScale = Render::Textures::Upscale::ReadScale(jpegBuf, fileTextureWidth, fileTextureHeight);
    const bool upscaled = fileScale > Render::Textures::Upscale::NoScale && !keepPixels;
    namespace HdSize = Render::Textures::HdSize;
    const bool halve = upscaled && HdSize::Divisor(HdSize::Half(), fileScale, jpegWidth, jpegHeight) == 2 &&
                       HdSize::Divisor(true, fileScale, fileTextureWidth, fileTextureHeight) == 2;
    const int divisor = halve ? 2 : 1;
    jpegWidth /= divisor;
    jpegHeight /= divisor;

    std::vector<unsigned char> decompressedBuffer(static_cast<std::size_t>(jpegWidth) *
                                                  static_cast<std::size_t>(jpegHeight) * 3u);
    auto decompressResult = tjDecompress2(tjHandle.get(), jpegData, jpegSize, decompressedBuffer.data(), jpegWidth, 0,
                                          jpegHeight, TJPF_RGB, TJFLAG_FASTDCT);
    if (decompressResult != 0)
    {
        ReportTurboError(L"tjDecompress2");
        return false;
    }

    const int textureWidth = fileTextureWidth / divisor;
    const int textureHeight = fileTextureHeight / divisor;
    const int scale = fileScale / divisor;

    auto pNewBitmap = CreateBitmapStorage(uiBitmapIndex, filename, textureWidth, textureHeight, 3);
    pNewBitmap->Width = static_cast<float>(textureWidth / scale);
    pNewBitmap->Height = static_cast<float>(textureHeight / scale);

    const int jpegRowSize = jpegWidth * 3;
    const int textureRowSize = textureWidth * 3;
    const int rows = std::min<int>(jpegHeight, textureHeight);

    std::size_t offset = 0;
    if (jpegWidth != textureWidth)
    {
        for (int row = 0; row < rows; ++row)
        {
            memcpy(&pNewBitmap->Buffer[offset], &decompressedBuffer[static_cast<std::size_t>(row) * jpegRowSize],
                   static_cast<std::size_t>(jpegRowSize));
            offset += static_cast<std::size_t>(textureRowSize);
        }
    }
    else
    {
        memcpy(pNewBitmap->Buffer, decompressedBuffer.data(),
               static_cast<std::size_t>(jpegHeight) * static_cast<std::size_t>(jpegWidth) * 3u);
    }

    int uploadWidth = textureWidth;
    int uploadHeight = textureHeight;
    if (keepPixels && scale > Render::Textures::Upscale::NoScale)
    {
        ShrinkCanvas(*pNewBitmap, uploadWidth, uploadHeight, scale);
    }

    pNewBitmap->TextureNumber = uiBitmapIndex;
    pNewBitmap->worldTexture = worldTexture;
    pNewBitmap->wrapRepeat = uiWrapMode == GL_REPEAT;
    pNewBitmap->scale = static_cast<std::uint8_t>(upscaled ? scale : 1);
    std::vector<std::uint8_t> rgbaData = PadRGBToRGBA(pNewBitmap->Buffer, uploadWidth, uploadHeight);
    if (m_pixelChange != nullptr)
    {
        (*m_pixelChange)(rgbaData.data(), rgbaData.size() / 4u, 4);
    }
    const MipLevels mips = PlanMipLevels(upscaled || (worldTexture && m_textureFilteringEnabled), rgbaData.data(),
                                         uploadWidth, uploadHeight, false, m_mipChainSeconds, upscaled);
    if (!UploadTextureSDLGpu(pNewBitmap.get(), rgbaData, uploadWidth, uploadHeight, MapGLFilterToSDL(uiFilter),
                             MapGLWrapToSDL(uiWrapMode), mips))
    {
        return false;
    }
    pNewBitmap->UploadedBytes = UploadedBytes(uploadWidth, uploadHeight);
    if (upscaled)
    {
        UploadNormalMap(*pNewBitmap, filename_ozj, rgbaData, uploadWidth, uploadHeight, scale, divisor,
                        MapGLWrapToSDL(uiWrapMode));
    }
    if (!keepPixels)
    {
        ReleaseUploadedPixels(*pNewBitmap);
    }

    SDL_GPUTexture* rawTexture = pNewBitmap->sdlTexture;
    SDL_GPUSampler* rawSampler = pNewBitmap->sdlSampler;
    SDL_GPUTexture* normalMap = pNewBitmap->sdlNormalMap;
    const BITMAP_t& loaded = *pNewBitmap;
    const std::uint32_t uploadedBytes = pNewBitmap->UploadedBytes;
    m_mapBitmap.insert(type_bitmap_map::value_type(uiBitmapIndex, std::move(pNewBitmap)));
    mu::RegisterTexture(uiBitmapIndex, rawTexture);
    mu::RegisterSampler(uiBitmapIndex, rawSampler);
    if (normalMap)
    {
        mu::RegisterNormalMap(uiBitmapIndex, normalMap);
    }
    RegisterFilteredSampler(loaded);
    m_dwUsedTextureMemory += uploadedBytes;

    return true;
}

bool CGlobalBitmap::OpenTga(GLuint uiBitmapIndex, const std::wstring& filename, GLuint uiFilter, GLuint uiWrapMode,
                            bool worldTexture, bool padToPowerOfTwo, bool premultiplyAlpha, bool keepPixels)
{
    namespace Ozt = Render::Textures::Ozt;

    std::wstring classic_ozt;
    ExchangeExt(filename, L"OZT", classic_ozt);
    std::wstring filename_ozt = keepPixels ? classic_ozt : Render::Textures::Hd::Resolve(classic_ozt);

    // The file's bytes; the HD art ships packed (scripts/hd_pack.py), so back to the original file first.
    const auto readOzt = [](const std::wstring& path, std::vector<unsigned char>& bytes)
    {
        std::ifstream input(Render::Sprites::NarrowBitmapPath(path), std::ios::binary);
        if (!input)
        {
            mu::log::Get("render")->debug("OpenTga -- file open failed: {}", mu_wchar_to_utf8(path.c_str()));
            return false;
        }
        bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (!Render::Textures::PackedOzt::IsPacked(bytes))
        {
            return true;
        }
        std::vector<unsigned char> unpacked;
        if (!Render::Textures::PackedOzt::Unpack(bytes, unpacked))
        {
            g_ErrorReport.Write(L"OpenTga: broken packed file %ls (%zu bytes)\r\n", path.c_str(), bytes.size());
            return false;
        }
        bytes = std::move(unpacked);
        return true;
    };

    std::vector<unsigned char> pakBuffer;
    if (!readOzt(filename_ozt, pakBuffer))
    {
        // A missing or broken HD copy: the original texture instead.
        if (filename_ozt == classic_ozt || !readOzt(classic_ozt, pakBuffer))
        {
            return false;
        }
        g_ErrorReport.Write(L"OpenTga: HD copy unusable, original used: %ls\r\n", classic_ozt.c_str());
        filename_ozt = classic_ozt;
    }

    // A marked file holds an image up to its scale times the largest original.
    const int markedScale = Render::Textures::Upscale::ReadMarkedScale(pakBuffer);
    const int maxWidth = Render::Sprites::kBitmapMaximumWidth * markedScale;
    const int maxHeight = Render::Sprites::kBitmapMaximumHeight * markedScale;

    // A file cut off in its header or its pixels is too small.
    const Ozt::Header header = Ozt::ReadHeader(pakBuffer, maxWidth, maxHeight);
    if (header.status == Ozt::Status::TooSmall)
    {
        g_ErrorReport.Write(L"OpenTga: file too small %ls (%zu bytes)\r\n", filename_ozt.c_str(), pakBuffer.size());
        return false;
    }
    if (header.status == Ozt::Status::InvalidFormat)
    {
        g_ErrorReport.Write(L"OpenTga: invalid format %ls (bit=%d, %dx%d, at most %dx%d)\r\n", filename_ozt.c_str(),
                            header.bitsPerPixel, header.width, header.height, maxWidth, maxHeight);
        return false;
    }

    const int Width = padToPowerOfTwo ? NextPowerOfTwo(header.width, maxWidth) : header.width;
    const int Height = padToPowerOfTwo ? NextPowerOfTwo(header.height, maxHeight) : header.height;
    const int fileScale = Render::Textures::Upscale::ReadScale(pakBuffer, Width, Height);
    const bool upscaled = fileScale > Render::Textures::Upscale::NoScale && !keepPixels;
    // With HD textures at half size an HD copy is uploaded at half its sides (HdTextureSize.h).
    namespace HdSize = Render::Textures::HdSize;
    const int divisor = upscaled ? HdSize::Divisor(HdSize::Half(), fileScale, Width, Height) : 1;
    const int scale = fileScale / divisor;

    auto pNewBitmap = CreateBitmapStorage(uiBitmapIndex, filename, Width, Height, 4);
    pNewBitmap->Width = static_cast<float>(Width / fileScale);
    pNewBitmap->Height = static_cast<float>(Height / fileScale);
    const std::size_t BufferSize = pNewBitmap->BufferStorage.size();

    Ozt::CopyPixels(pakBuffer, header, std::span<std::uint8_t>(pNewBitmap->Buffer, BufferSize), Width);
    if (m_pixelChange != nullptr)
    {
        (*m_pixelChange)(pNewBitmap->Buffer, BufferSize / Ozt::BytesPerPixel, Ozt::BytesPerPixel);
    }
    if (premultiplyAlpha)
    {
        Render::RmlUi::PremultiplyAlpha(pNewBitmap->Buffer, BufferSize / Ozt::BytesPerPixel);
    }

    int uploadWidth = Width;
    int uploadHeight = Height;
    if (keepPixels && scale > Render::Textures::Upscale::NoScale)
    {
        ShrinkCanvas(*pNewBitmap, uploadWidth, uploadHeight, scale);
    }
    else if (divisor == 2)
    {
        // Colour weighted by alpha, so cut-out edges keep clear of the colour of the hidden texels.
        std::vector<std::uint8_t> half =
            HdSize::HalveRgba(pNewBitmap->BufferStorage, Width, Height, !premultiplyAlpha);
        if (half.empty())
        {
            g_ErrorReport.Write(L"OpenTga: half size failed %ls (%dx%d)\r\n", filename_ozt.c_str(), Width, Height);
            return false;
        }
        pNewBitmap->BufferStorage = std::move(half);
        pNewBitmap->Buffer = pNewBitmap->BufferStorage.data();
        uploadWidth /= 2;
        uploadHeight /= 2;
    }

    pNewBitmap->TextureNumber = uiBitmapIndex;
    pNewBitmap->worldTexture = worldTexture;
    pNewBitmap->wrapRepeat = uiWrapMode == GL_REPEAT;
    pNewBitmap->mapCutout =
        IsMapCutout(uiBitmapIndex, filename, pNewBitmap->BufferStorage, uploadWidth, uploadHeight);
    pNewBitmap->scale = static_cast<std::uint8_t>(upscaled ? scale : 1);
    const MipLevels mips = PlanMipLevels(upscaled || (worldTexture && m_textureFilteringEnabled), pNewBitmap->Buffer,
                                         uploadWidth, uploadHeight, true, m_mipChainSeconds);
    if (!UploadTextureSDLGpu(pNewBitmap.get(), pNewBitmap->BufferStorage, uploadWidth, uploadHeight,
                             MapGLFilterToSDL(uiFilter), MapGLWrapToSDL(uiWrapMode), mips))
    {
        return false;
    }
    pNewBitmap->UploadedBytes = UploadedBytes(uploadWidth, uploadHeight);
    if (upscaled)
    {
        UploadNormalMap(*pNewBitmap, filename_ozt, pNewBitmap->BufferStorage, uploadWidth, uploadHeight, scale,
                        divisor, MapGLWrapToSDL(uiWrapMode));
    }
    if (!keepPixels)
    {
        ReleaseUploadedPixels(*pNewBitmap);
    }

    SDL_GPUTexture* rawTexture = pNewBitmap->sdlTexture;
    SDL_GPUSampler* rawSampler = pNewBitmap->sdlSampler;
    SDL_GPUTexture* normalMap = pNewBitmap->sdlNormalMap;
    const BITMAP_t& loaded = *pNewBitmap;
    const std::uint32_t uploadedBytes = pNewBitmap->UploadedBytes;
    m_mapBitmap.insert(type_bitmap_map::value_type(uiBitmapIndex, std::move(pNewBitmap)));
    mu::RegisterTexture(uiBitmapIndex, rawTexture);
    mu::RegisterSampler(uiBitmapIndex, rawSampler);
    if (normalMap)
    {
        mu::RegisterNormalMap(uiBitmapIndex, normalMap);
    }
    RegisterFilteredSampler(loaded);
    m_dwUsedTextureMemory += uploadedBytes;

    return true;
}

void CGlobalBitmap::SetTextureFiltering(bool enabled, int anisotropy)
{
    m_textureFilteringRequested = enabled;
    m_requestedAnisotropy = anisotropy;
    m_textureFilteringSwitchPending = true;
}

void CGlobalBitmap::SetCutoutMipBias(std::optional<float> bias)
{
    if (bias == m_requestedCutoutMipBias)
    {
        return;
    }
    m_requestedCutoutMipBias = bias;
    m_cutoutBiasSwitchPending = true;
}

void CGlobalBitmap::UpdateTextureFiltering()
{
    if (m_textureFilteringSwitchPending)
    {
        ApplyTextureFilteringSwitch();
    }
    if (m_cutoutBiasSwitchPending)
    {
        ApplyCutoutBiasSwitch();
    }
    if (!m_textureFilteringEnabled)
    {
        return;
    }

    mu::TakeFilteredSamplerRequests(m_mipChainRequests);
    if (m_mipChainRequests.empty())
    {
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    std::size_t built = 0;
    while (built < m_mipChainRequests.size())
    {
        BuildMipChain(m_mipChainRequests[built++]);
        if (std::chrono::steady_clock::now() - start >= MipChainBudgetPerFrame)
        {
            break;
        }
    }
    m_mipChainRequests.erase(m_mipChainRequests.begin(),
                             m_mipChainRequests.begin() + static_cast<std::ptrdiff_t>(built));
}

void CGlobalBitmap::ApplyTextureFilteringSwitch()
{
    m_textureFilteringSwitchPending = false;

    const Render::Textures::AnisotropyLevel anisotropy = Render::Textures::ClampAnisotropy(m_requestedAnisotropy);
    if (anisotropy.clamped && !m_anisotropyClampLogged)
    {
        mu::log::Get("render")->warn("Texture filtering: anisotropy {} is above the highest level the GPU supports; "
                                     "using {}",
                                     m_requestedAnisotropy, anisotropy.value);
        m_anisotropyClampLogged = true;
    }
    if (anisotropy.value != m_anisotropy)
    {
        // The shared samplers bake in the level; rebuild them and point every
        // chained texture at the new ones.
        ReleaseFilteredSamplers();
        m_anisotropy = anisotropy.value;
        for (const auto& [index, bitmap] : m_mapBitmap)
        {
            RegisterFilteredSampler(*bitmap);
        }
    }

    const bool changed = m_textureFilteringEnabled != m_textureFilteringRequested;
    m_textureFilteringEnabled = m_textureFilteringRequested;
    mu::SetTextureFilteringEnabled(m_textureFilteringEnabled);
    m_mipChainRequests.clear();
    if (m_textureFilteringEnabled)
    {
        // World textures loaded while Classic have no chain yet: build each one
        // the first time a world draw uses it.
        for (const auto& [index, bitmap] : m_mapBitmap)
        {
            // A recorded file gets its chain when it loads.
            if (bitmap->worldTexture && bitmap->mipLevels <= 1u && !bitmap->pendingLoad)
            {
                mu::MarkFilteredSamplerPending(index);
            }
        }
    }

    if (changed)
    {
        LogTextureFilteringSummary(m_textureFilteringEnabled ? "switched to Enhanced" : "switched to Classic");
    }
}

void CGlobalBitmap::ApplyCutoutBiasSwitch()
{
    m_cutoutBiasSwitchPending = false;
    if (m_requestedCutoutMipBias == m_cutoutMipBias)
    {
        return;
    }
    // The cut-out samplers bake in the bias; drop them and point every chained cut-out at the new
    // ones (or back at the shared Enhanced ones when there is none).
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(mu::GetRenderer().GetDevice());
    for (std::size_t i = kCutoutSamplerFirst; i < m_filteredSamplers.size(); ++i)
    {
        if (m_filteredSamplers[i] && device)
        {
            SDL_ReleaseGPUSampler(device, m_filteredSamplers[i]);
        }
        m_filteredSamplers[i] = nullptr;
    }
    m_cutoutMipBias = m_requestedCutoutMipBias;
    for (const auto& [index, bitmap] : m_mapBitmap)
    {
        if (bitmap->mapCutout)
        {
            RegisterFilteredSampler(*bitmap);
        }
    }
}

void CGlobalBitmap::RegisterFilteredSampler(const BITMAP_t& bitmap)
{
    const bool upscaled = bitmap.scale > 1u;
    if ((!bitmap.worldTexture && !upscaled) || bitmap.mipLevels <= 1u)
    {
        return;
    }
    const bool cutout = m_cutoutMipBias.has_value() && bitmap.mapCutout;
    SDL_GPUSampler* sampler = GetFilteredSampler(bitmap.wrapRepeat, cutout);
    if (!sampler && cutout)
    {
        // No cut-out sampler: the shared Enhanced one, so no registry keeps a sampler released before.
        sampler = GetFilteredSampler(bitmap.wrapRepeat, false);
    }
    if (!sampler)
    {
        return;
    }
    if (bitmap.worldTexture)
    {
        mu::RegisterFilteredSampler(bitmap.BitmapIndex, sampler);
    }
    // Upscaled art is drawn smaller than it is stored in both looks.
    if (upscaled)
    {
        mu::RegisterSampler(bitmap.BitmapIndex, sampler);
    }
}

SDL_GPUSampler* CGlobalBitmap::GetFilteredSampler(bool wrapRepeat, bool cutout)
{
    const bool cutoutSampler = cutout && m_cutoutMipBias.has_value();
    SDL_GPUSampler*& sampler =
        m_filteredSamplers[(cutoutSampler ? kCutoutSamplerFirst : 0u) + (wrapRepeat ? 1u : 0u)];
    if (sampler)
    {
        return sampler;
    }

    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(mu::GetRenderer().GetDevice());
    if (!device)
    {
        return nullptr;
    }

    const SDL_GPUSamplerAddressMode wrapMode =
        wrapRepeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    const std::uint32_t anisotropy = std::max<std::uint32_t>(1u, m_anisotropy);

    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    samplerInfo.address_mode_u = wrapMode;
    samplerInfo.address_mode_v = wrapMode;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.min_lod = 0.0f;
    samplerInfo.max_lod = UnclampedMaxLod;
    samplerInfo.mip_lod_bias =
        cutoutSampler ? *m_cutoutMipBias : Render::Textures::MipLodBias(Render::Textures::Hd::Enabled());
    samplerInfo.enable_anisotropy = anisotropy > 1u;
    samplerInfo.max_anisotropy = static_cast<float>(anisotropy);

    sampler = SDL_CreateGPUSampler(device, &samplerInfo);
    if (!sampler)
    {
        mu::log::Get("render")->error("Texture filtering: creating the {} sampler failed: {}",
                                      cutoutSampler ? "foliage cut-out" : "Enhanced", SDL_GetError());
    }
    return sampler;
}

void CGlobalBitmap::ReleaseFilteredSamplers()
{
    // The static Bitmaps runs this again at exit, after the renderer and the logger are gone.
    if (std::none_of(m_filteredSamplers.begin(), m_filteredSamplers.end(),
                     [](const SDL_GPUSampler* sampler) { return sampler != nullptr; }))
    {
        return;
    }
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(mu::GetRenderer().GetDevice());
    for (SDL_GPUSampler*& sampler : m_filteredSamplers)
    {
        if (sampler && device)
        {
            SDL_ReleaseGPUSampler(device, sampler);
        }
        sampler = nullptr;
    }
}

bool CGlobalBitmap::ReadBitmapPixels(const BITMAP_t& bitmap, std::vector<std::uint8_t>& rgba)
{
    namespace Ozt = Render::Textures::Ozt;

    const std::wstring filename = bitmap.FileName;
    std::wstring ext;
    SplitExt(filename, ext, false);
    const bool jpeg = 0 == _wcsicmp(ext.c_str(), L"jpg");
    if (!jpeg && 0 != _wcsicmp(ext.c_str(), L"tga"))
    {
        return false;
    }
    std::wstring packed;
    ExchangeExt(filename, jpeg ? L"OZJ" : L"OZT", packed);
    std::ifstream input(Render::Sprites::NarrowBitmapPath(packed), std::ios::binary);
    if (!input)
    {
        return false;
    }
    const std::vector<unsigned char> file((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    // The texture as loaded: an original image, padded to this size.
    const auto width = static_cast<int>(bitmap.Width);
    const auto height = static_cast<int>(bitmap.Height);
    rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0u);
    if (!jpeg)
    {
        const Ozt::Header header = Ozt::ReadHeader(file, width, height);
        if (header.status != Ozt::Status::Valid)
        {
            return false;
        }
        Ozt::CopyPixels(file, header, rgba, width);
        return true;
    }

    TurboJpegHandle tjHandle;
    int jpegWidth = 0, jpegHeight = 0, jpegSubsamp = 0, jpegColorspace = 0;
    if (file.size() <= 24 || !tjHandle.valid())
    {
        return false;
    }
    const unsigned char* jpegData = file.data() + 24;
    const auto jpegSize = static_cast<unsigned long>(file.size() - 24);
    if (tjDecompressHeader3(tjHandle.get(), jpegData, jpegSize, &jpegWidth, &jpegHeight, &jpegSubsamp,
                            &jpegColorspace) != 0 ||
        jpegWidth <= 0 || jpegHeight <= 0 || jpegWidth > width || jpegHeight > height)
    {
        return false;
    }
    std::vector<unsigned char> rgb(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u, 0u);
    if (tjDecompress2(tjHandle.get(), jpegData, jpegSize, rgb.data(), jpegWidth, width * 3, jpegHeight, TJPF_RGB,
                      TJFLAG_FASTDCT) != 0)
    {
        return false;
    }
    rgba = PadRGBToRGBA(rgb.data(), width, height);
    return true;
}

bool CGlobalBitmap::BuildMipChain(GLuint uiBitmapIndex)
{
    auto found = m_mapBitmap.find(uiBitmapIndex);
    if (found == m_mapBitmap.end())
    {
        return false;
    }
    BITMAP_t& bitmap = *found->second;
    const auto width = static_cast<int>(bitmap.Width);
    const auto height = static_cast<int>(bitmap.Height);
    SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(mu::GetRenderer().GetDevice());
    // HD art (scale above 1) got its whole chain at load; its pixels are not the original file's.
    if (!device || !bitmap.worldTexture || bitmap.mipLevels > 1u || bitmap.scale > 1u || width <= 0 || height <= 0 ||
        (bitmap.Components != 3 && bitmap.Components != 4))
    {
        return false;
    }

    // The pixels were released after the upload (a canvas keeps its own).
    const bool hasAlpha = bitmap.Components == 4;
    std::vector<std::uint8_t> rgba;
    if (bitmap.Buffer)
    {
        rgba = hasAlpha ? std::vector<std::uint8_t>(bitmap.Buffer, bitmap.Buffer + bitmap.BufferStorage.size())
                        : PadRGBToRGBA(bitmap.Buffer, width, height);
    }
    else if (!ReadBitmapPixels(bitmap, rgba))
    {
        return false;
    }
    const MipLevels mips = PlanMipLevels(true, rgba.data(), width, height, hasAlpha, m_mipChainSeconds);

    // Only the texture is replaced: the bitmap keeps its Classic sampler, so the
    // filter passed here does not matter.
    const auto result = mu::UploadBitmapPixels(
        rgba, width, height, SDL_GPU_FILTER_LINEAR,
        bitmap.wrapRepeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, mips.chain,
        mips.generate);
    if (!result)
    {
        mu::log::Get("render")->error("Texture filtering: creating the mipmapped texture {} failed at {}: {}",
                                      mu_wchar_to_utf8(bitmap.FileName), result.stage, result.error);
        return false;
    }
    SDL_GPUTexture* texture = result.texture;

    // Runs between frames, so no recorded draw still refers to the old texture.
    SDL_GPUTexture* previous = bitmap.sdlTexture;
    bitmap.sdlTexture = texture;
    bitmap.mipLevels = static_cast<std::uint8_t>(
        mips.generate ? Render::Textures::CountMipLevels(static_cast<std::uint32_t>(width),
                                                         static_cast<std::uint32_t>(height))
                      : mips.chain.size() + 1u);
    mu::RegisterTexture(uiBitmapIndex, texture);
    if (previous)
    {
        mu::ReleaseBitmapTexture(device, previous);
    }
    RegisterFilteredSampler(bitmap);
    return true;
}

void CGlobalBitmap::LogTextureFilteringSummary(const char* context) const
{
    std::size_t world = 0;
    std::size_t mipmapped = 0;
    std::size_t screenSpace = 0;
    std::uint64_t gpuBytes = 0;
    for (const auto& [index, bitmap] : m_mapBitmap)
    {
        if (bitmap->pendingLoad)
        {
            continue;
        }
        if (bitmap->worldTexture)
        {
            ++world;
        }
        else
        {
            ++screenSpace;
        }
        if (bitmap->mipLevels > 1u)
        {
            ++mipmapped;
        }
        gpuBytes += GpuTextureBytes(*bitmap);
    }

    mu::log::Get("render")->info(
        "Texture filtering ({}): {}, anisotropy {}; {} world textures ({} mipmapped), {} screen-space; "
        "{:.1f} MiB on the GPU; {} more wait for their first use; texture loading {:.0f} ms, of which mip "
        "chains {:.0f} ms",
        context, m_textureFilteringEnabled ? "Enhanced" : "Classic", m_anisotropy, world, mipmapped, screenSpace,
        static_cast<double>(gpuBytes) / (1024.0 * 1024.0), m_pendingLoads, m_textureLoadSeconds * 1000.0,
        m_mipChainSeconds * 1000.0);
}

void CGlobalBitmap::SplitExt(IN const std::wstring& filepath, OUT std::wstring& ext, bool bIncludeDot)
{
    wchar_t __ext[_MAX_EXT] = {
        0,
    };
    _wsplitpath(filepath.c_str(), NULL, NULL, NULL, __ext);
    if (bIncludeDot)
    {
        ext = __ext;
    }
    else
    {
        if ((__ext[0] == '.') && __ext[1])
            ext = __ext + 1;
    }
}
void CGlobalBitmap::ExchangeExt(IN const std::wstring& in_filepath, IN const std::wstring& ext,
                                OUT std::wstring& out_filepath)
{
    wchar_t __drive[_MAX_DRIVE] = {
        0,
    };
    wchar_t __dir[_MAX_DIR] = {
        0,
    };
    wchar_t __fname[_MAX_FNAME] = {
        0,
    };
    _wsplitpath(in_filepath.c_str(), __drive, __dir, __fname, NULL);

    out_filepath = __drive;
    out_filepath += __dir;
    out_filepath += __fname;
    out_filepath += '.';
    out_filepath += ext;
}
