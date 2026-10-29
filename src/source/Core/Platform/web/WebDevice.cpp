// Web build only: see WebDevice.h.

#include "Core/Platform/web/WebDevice.h"

#include <emscripten.h>

namespace
{
// Module.muDevice: "phone", "tablet" or "desktop". A page without it (an older index.html) means a
// computer, as the game was before. (Loose ==: clang-format splits "===".)
EM_JS(int, MuWeb_GetDevice, (), {
    if (Module.muDevice == "phone")
        return 2;
    return Module.muDevice == "tablet" ? 1 : 0;
});

mu::web::Device ReadDevice()
{
    switch (MuWeb_GetDevice())
    {
    case 2:
        return mu::web::Device::Phone;
    case 1:
        return mu::web::Device::Tablet;
    default:
        return mu::web::Device::Desktop;
    }
}
} // namespace

mu::web::Device mu::web::GetDevice()
{
    // The page decides once, before the game starts.
    static const Device device = ReadDevice();
    return device;
}

bool mu::web::IsTouchDevice()
{
    return GetDevice() != Device::Desktop;
}

const char* mu::web::DeviceName(Device device)
{
    switch (device)
    {
    case Device::Phone:
        return "phone";
    case Device::Tablet:
        return "tablet";
    default:
        return "desktop";
    }
}
