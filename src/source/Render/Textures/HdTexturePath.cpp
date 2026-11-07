#include "Render/Textures/HdTextures.h"

#include <cwctype>

namespace Render::Textures::Hd
{
namespace
{
bool IsSeparator(wchar_t c)
{
    return c == L'\\' || c == L'/';
}

// Whether `path` starts with `folder` followed by a separator, in any case.
bool StartsWithFolder(std::wstring_view path, std::wstring_view folder)
{
    if (path.size() <= folder.size() || !IsSeparator(path[folder.size()]))
    {
        return false;
    }
    for (std::size_t i = 0; i < folder.size(); ++i)
    {
        if (std::towlower(path[i]) != std::towlower(folder[i]))
        {
            return false;
        }
    }
    return true;
}
} // namespace

std::optional<std::wstring> HdCopyPath(std::wstring_view path)
{
    if (!StartsWithFolder(path, L"Data"))
    {
        return std::nullopt;
    }
    const wchar_t separator = path[4];
    const std::wstring_view rest = path.substr(5);
    if (rest.empty() || StartsWithFolder(rest, L"HD"))
    {
        return std::nullopt;
    }
    std::wstring copy(path.substr(0, 5));
    copy += L"HD";
    copy += separator;
    copy += rest;
    return copy;
}

} // namespace Render::Textures::Hd
