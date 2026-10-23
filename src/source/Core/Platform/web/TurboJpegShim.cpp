// Web build only: the TurboJPEG calls the client makes, on top of SDL3_image (decoding, and
// encoding for screenshots). See Core/Platform/web/include/turbojpeg.h.

#include "turbojpeg.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{
// The client asks for the header, then decompresses the same buffer: the handle keeps the image
// decoded for the header so the second call doesn't decode it again.
struct ShimHandle
{
    const unsigned char* buffer = nullptr;
    unsigned long size = 0;
    SDL_Surface* rgb = nullptr;
};

thread_local std::string g_lastError;

int Fail(const char* what)
{
    g_lastError = std::string(what) + ": " + SDL_GetError();
    return -1;
}

SDL_Surface* Decode(ShimHandle* handle, const unsigned char* jpegBuf, unsigned long jpegSize)
{
    if (handle->rgb && handle->buffer == jpegBuf && handle->size == jpegSize)
    {
        return handle->rgb;
    }

    SDL_DestroySurface(handle->rgb);
    handle->rgb = nullptr;
    SDL_IOStream* io = SDL_IOFromConstMem(jpegBuf, static_cast<size_t>(jpegSize));
    SDL_Surface* decoded = io ? IMG_LoadTyped_IO(io, true, "JPG") : nullptr;
    if (!decoded)
    {
        return nullptr;
    }

    handle->rgb = SDL_ConvertSurface(decoded, SDL_PIXELFORMAT_RGB24);
    SDL_DestroySurface(decoded);
    handle->buffer = jpegBuf;
    handle->size = jpegSize;
    return handle->rgb;
}
} // namespace

extern "C" {

tjhandle tjInitDecompress(void)
{
    return new ShimHandle();
}

tjhandle tjInitCompress(void)
{
    return new ShimHandle();
}

int tjDestroy(tjhandle handle)
{
    auto* shim = static_cast<ShimHandle*>(handle);
    if (shim)
    {
        SDL_DestroySurface(shim->rgb);
        delete shim;
    }
    return 0;
}

char* tjGetErrorStr(void)
{
    return g_lastError.data();
}

int tjDecompressHeader3(tjhandle handle, const unsigned char* jpegBuf, unsigned long jpegSize, int* width,
                        int* height, int* jpegSubsamp, int* jpegColorspace)
{
    SDL_Surface* rgb = Decode(static_cast<ShimHandle*>(handle), jpegBuf, jpegSize);
    if (!rgb)
    {
        return Fail("tjDecompressHeader3");
    }

    *width = rgb->w;
    *height = rgb->h;
    if (jpegSubsamp)
    {
        *jpegSubsamp = TJSAMP_444;
    }
    if (jpegColorspace)
    {
        *jpegColorspace = TJCS_RGB;
    }
    return 0;
}

int tjDecompress2(tjhandle handle, const unsigned char* jpegBuf, unsigned long jpegSize, unsigned char* dstBuf,
                  int width, int pitch, int height, int pixelFormat, int flags)
{
    SDL_Surface* rgb = Decode(static_cast<ShimHandle*>(handle), jpegBuf, jpegSize);
    if (!rgb)
    {
        return Fail("tjDecompress2");
    }
    if (pixelFormat != TJPF_RGB || width != rgb->w || height != rgb->h)
    {
        g_lastError = "tjDecompress2: only full-size RGB output is supported";
        return -1;
    }

    const int rowBytes = width * 3;
    const int dstPitch = pitch == 0 ? rowBytes : pitch;
    const bool bottomUp = (flags & TJFLAG_BOTTOMUP) != 0;
    const auto* src = static_cast<const unsigned char*>(rgb->pixels);
    for (int y = 0; y < height; ++y)
    {
        const int dstRow = bottomUp ? height - 1 - y : y;
        std::memcpy(dstBuf + static_cast<size_t>(dstRow) * dstPitch, src + static_cast<size_t>(y) * rgb->pitch,
                    rowBytes);
    }
    return 0;
}

unsigned long tjBufSize(int width, int height, int /*jpegSubsamp*/)
{
    // libjpeg-turbo's own worst case for 4:4:4, plus its fixed overhead.
    return static_cast<unsigned long>(width) * height * 3 + 2048;
}

int tjCompress2(tjhandle /*handle*/, const unsigned char* srcBuf, int width, int pitch, int height, int pixelFormat,
                unsigned char** jpegBuf, unsigned long* jpegSize, int /*jpegSubsamp*/, int jpegQual, int flags)
{
    if (pixelFormat != TJPF_RGB)
    {
        g_lastError = "tjCompress2: only RGB input is supported";
        return -1;
    }

    const int rowBytes = width * 3;
    const int srcPitch = pitch == 0 ? rowBytes : pitch;
    std::vector<unsigned char> rows(static_cast<size_t>(rowBytes) * height);
    const bool bottomUp = (flags & TJFLAG_BOTTOMUP) != 0;
    for (int y = 0; y < height; ++y)
    {
        const int srcRow = bottomUp ? height - 1 - y : y;
        std::memcpy(rows.data() + static_cast<size_t>(y) * rowBytes, srcBuf + static_cast<size_t>(srcRow) * srcPitch,
                    rowBytes);
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGB24, rows.data(), rowBytes);
    SDL_IOStream* io = SDL_IOFromDynamicMem();
    const bool saved = surface && io && IMG_SaveJPG_IO(surface, io, false, jpegQual);
    std::vector<unsigned char> encoded;
    if (saved)
    {
        encoded.resize(static_cast<size_t>(SDL_TellIO(io)));
        SDL_SeekIO(io, 0, SDL_IO_SEEK_SET);
        SDL_ReadIO(io, encoded.data(), encoded.size());
    }
    SDL_CloseIO(io);
    SDL_DestroySurface(surface);
    if (!saved)
    {
        return Fail("tjCompress2");
    }

    if (flags & TJFLAG_NOREALLOC)
    {
        if (encoded.size() > *jpegSize)
        {
            g_lastError = "tjCompress2: output buffer too small";
            return -1;
        }
    }
    else
    {
        *jpegBuf = static_cast<unsigned char*>(SDL_malloc(encoded.size()));
    }
    std::memcpy(*jpegBuf, encoded.data(), encoded.size());
    *jpegSize = static_cast<unsigned long>(encoded.size());
    return 0;
}

} // extern "C"
