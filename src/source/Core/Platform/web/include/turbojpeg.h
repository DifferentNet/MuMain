#pragma once

// Web build only. The browser build has no libjpeg-turbo, so this declares the slice of its
// TurboJPEG API that the client uses; Core/Platform/web/TurboJpegShim.cpp implements it with
// SDL3_image (decoding) and stb_image_write (encoding). Values match libjpeg-turbo's.

typedef void* tjhandle;

#define TJPF_RGB 0
#define TJSAMP_444 0
#define TJCS_RGB 0

#define TJFLAG_BOTTOMUP 2
#define TJFLAG_NOREALLOC 1024
#define TJFLAG_FASTDCT 2048

#ifdef __cplusplus
extern "C" {
#endif

tjhandle tjInitDecompress(void);
tjhandle tjInitCompress(void);
int tjDestroy(tjhandle handle);
char* tjGetErrorStr(void);

int tjDecompressHeader3(tjhandle handle, const unsigned char* jpegBuf, unsigned long jpegSize, int* width,
                        int* height, int* jpegSubsamp, int* jpegColorspace);
int tjDecompress2(tjhandle handle, const unsigned char* jpegBuf, unsigned long jpegSize, unsigned char* dstBuf,
                  int width, int pitch, int height, int pixelFormat, int flags);

unsigned long tjBufSize(int width, int height, int jpegSubsamp);
int tjCompress2(tjhandle handle, const unsigned char* srcBuf, int width, int pitch, int height, int pixelFormat,
                unsigned char** jpegBuf, unsigned long* jpegSize, int jpegSubsamp, int jpegQual, int flags);

#ifdef __cplusplus
}
#endif
