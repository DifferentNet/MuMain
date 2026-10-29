// Web build only: see WebGpuFeatures.h.

#include "Core/Platform/web/WebGpuFeatures.h"

#include <emscripten.h>

namespace
{
// Module.muClipDistances, set by the page's GPU check. A page without it (an older index.html)
// means yes, as the game required the feature before. (Loose ==: clang-format splits "===".)
EM_JS(int, MuWeb_HasClipDistances, (), { return Module.muClipDistances == false ? 0 : 1; });
// Module.muDepthClamp, the same way.
EM_JS(int, MuWeb_HasDepthClamp, (), { return Module.muDepthClamp == false ? 0 : 1; });
} // namespace

bool mu::web::HasClipDistances()
{
    return MuWeb_HasClipDistances() != 0;
}

bool mu::web::HasDepthClamp()
{
    static const bool hasDepthClamp = MuWeb_HasDepthClamp() != 0;
    return hasDepthClamp;
}
