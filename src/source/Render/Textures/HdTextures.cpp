#include "stdafx.h"

#include "Render/Textures/HdTextures.h"

#include "Render/Sprites/GlobalBitmapPath.h"

#include <filesystem>
#include <system_error>

namespace Render::Textures::Hd
{
namespace
{
bool s_enabled = false;
} // namespace

void SetEnabled(bool enabled)
{
    s_enabled = enabled;
}

bool Enabled()
{
    return s_enabled;
}

std::wstring Resolve(const std::wstring& path)
{
    if (!s_enabled)
    {
        return path;
    }
    std::optional<std::wstring> copy = HdCopyPath(path);
    if (!copy)
    {
        return path;
    }
    std::error_code error;
    if (std::filesystem::is_regular_file(Render::Sprites::NarrowBitmapPath(*copy), error))
    {
        return *std::move(copy);
    }
    return path;
}

} // namespace Render::Textures::Hd
