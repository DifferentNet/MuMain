#include "stdafx.h"
#include "Core/Platform/PlatformLibrary.h"

#include <dlfcn.h>

#include "Core/Utilities/Log/MuLogger.h"

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebNetwork.h"

#include <cstring>

namespace
{
// Browser build: there is no dynamic loading. The one library the client loads,
// MUnique.Client.Library, is built in (Core/Platform/web/WebNetwork.cpp); this marks its handle.
char g_webNetworkLibrary;
} // namespace
#endif

namespace mu::platform
{

LibraryHandle Load(const char* path)
{
    if (path == nullptr)
    {
        mu::log::Get("platform")->error("PLAT: PlatformLibrary::Load() failed -- path is null");
        return nullptr;
    }

#ifdef __EMSCRIPTEN__
    if (std::strstr(path, "MUnique.Client.Library") != nullptr)
    {
        return &g_webNetworkLibrary;
    }
#endif

    void* handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr)
    {
        const char* error = dlerror();
        mu::log::Get("platform")
            ->error("PLAT: PlatformLibrary::Load() failed -- {} ({})", path, error ? error : "unknown error");
        return nullptr;
    }

    return handle;
}

void* GetSymbol(LibraryHandle handle, const char* name)
{
    if (handle == nullptr)
    {
        return nullptr;
    }

    if (name == nullptr)
    {
        mu::log::Get("platform")->error("PLAT: PlatformLibrary::GetSymbol() failed -- name is null");
        return nullptr;
    }

#ifdef __EMSCRIPTEN__
    if (handle == &g_webNetworkLibrary)
    {
        void* symbol = mu::web::FindNetworkSymbol(name);
        if (symbol == nullptr)
        {
            mu::log::Get("platform")->error("PLAT: PlatformLibrary::GetSymbol({}) failed -- not in the web network layer", name);
        }
        return symbol;
    }
#endif

    // Clear any existing error
    dlerror();

    void* symbol = dlsym(handle, name);

    // dlsym can return nullptr for a valid symbol that has value 0,
    // so we must check dlerror() to distinguish real errors.
    const char* error = dlerror();
    if (error != nullptr)
    {
        mu::log::Get("platform")->error("PLAT: PlatformLibrary::GetSymbol({}) failed -- {}", name, error);
        return nullptr;
    }

    return symbol;
}

void Unload(LibraryHandle handle)
{
    if (handle == nullptr)
    {
        return;
    }

#ifdef __EMSCRIPTEN__
    if (handle == &g_webNetworkLibrary)
    {
        return;
    }
#endif

    if (dlclose(handle) != 0)
    {
        const char* error = dlerror();
        mu::log::Get("platform")
            ->error("PLAT: PlatformLibrary::Unload() failed -- {}", error ? error : "unknown error");
    }
}

} // namespace mu::platform
