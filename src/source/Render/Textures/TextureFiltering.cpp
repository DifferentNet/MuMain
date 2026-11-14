#include "Render/Textures/TextureFiltering.h"

#include <algorithm>
#include <array>

namespace Render::Textures
{

namespace
{
[[nodiscard]] bool IsSeparator(wchar_t c)
{
    return c == L'\\' || c == L'/';
}

[[nodiscard]] wchar_t ToLowerAscii(wchar_t c)
{
    return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

[[nodiscard]] bool EqualsIgnoreCase(std::wstring_view text, std::wstring_view expected)
{
    return text.size() == expected.size() &&
           std::equal(text.begin(), text.end(), expected.begin(),
                      [](wchar_t a, wchar_t b) { return ToLowerAscii(a) == ToLowerAscii(b); });
}

[[nodiscard]] bool IsDigits(std::wstring_view text)
{
    return std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

// Splits off the first path component; `rest` keeps what follows its separator.
[[nodiscard]] std::wstring_view TakeComponent(std::wstring_view& rest)
{
    const auto separator = std::find_if(rest.begin(), rest.end(), IsSeparator);
    const auto length = static_cast<std::size_t>(separator - rest.begin());
    const std::wstring_view component = rest.substr(0, length);
    rest = separator == rest.end() ? std::wstring_view{} : rest.substr(length + 1u);
    return component;
}

constexpr std::array<std::wstring_view, 6> WorldFolders = {L"Player", L"Monster", L"Item", L"NPC", L"Effect", L"Skill"};

// A map's mini map picture (World<N>\mini_map.OZT): drawn in the HUD, never a cut-out.
constexpr std::wstring_view MiniMapFileStem = L"mini_map";

// Folders numbered per map: World1, Object1, ... (also accepted unnumbered).
constexpr std::array<std::wstring_view, 2> NumberedWorldFolders = {L"World", L"Object"};

// `folder` is `prefix` followed by digits only; `requireNumber` also asks for at least one digit.
[[nodiscard]] bool IsNumberedFolder(std::wstring_view folder, std::wstring_view prefix, bool requireNumber)
{
    if (folder.size() < prefix.size() || !EqualsIgnoreCase(folder.substr(0, prefix.size()), prefix))
    {
        return false;
    }
    const std::wstring_view number = folder.substr(prefix.size());
    return IsDigits(number) && (!requireNumber || !number.empty());
}

[[nodiscard]] bool IsWorldFolder(std::wstring_view folder)
{
    const auto named = [folder](std::wstring_view name) { return EqualsIgnoreCase(folder, name); };
    const auto numbered = [folder](std::wstring_view prefix) { return IsNumberedFolder(folder, prefix, false); };
    return std::any_of(WorldFolders.begin(), WorldFolders.end(), named) ||
           std::any_of(NumberedWorldFolders.begin(), NumberedWorldFolders.end(), numbered);
}

[[nodiscard]] bool IsMapFolder(std::wstring_view folder)
{
    const auto numbered = [folder](std::wstring_view prefix) { return IsNumberedFolder(folder, prefix, true); };
    return std::any_of(NumberedWorldFolders.begin(), NumberedWorldFolders.end(), numbered);
}

// The folder right under Data, with `rest` keeping the path after it; empty when `path` is not under Data.
[[nodiscard]] std::wstring_view DataFolder(std::wstring_view path, std::wstring_view& rest)
{
    rest = path;
    if (!EqualsIgnoreCase(TakeComponent(rest), L"Data") || rest.empty())
    {
        rest = {};
        return {};
    }
    return TakeComponent(rest);
}
} // namespace

bool IsWorldTexturePath(std::wstring_view path)
{
    std::wstring_view rest;
    const std::wstring_view folder = DataFolder(path, rest);
    return !rest.empty() && IsWorldFolder(folder);
}

bool IsMapTexturePath(std::wstring_view path)
{
    std::wstring_view rest;
    const std::wstring_view folder = DataFolder(path, rest);
    return !rest.empty() && IsMapFolder(folder);
}

bool IsMapCutoutCandidatePath(std::wstring_view path)
{
    if (!IsMapTexturePath(path))
    {
        return false;
    }
    const auto lastSeparator = std::find_if(path.rbegin(), path.rend(), IsSeparator);
    const std::wstring_view fileName = path.substr(static_cast<std::size_t>(path.rend() - lastSeparator));
    const std::wstring_view stem = fileName.substr(0, fileName.find(L'.'));
    return !EqualsIgnoreCase(stem, MiniMapFileStem);
}

AnisotropyLevel ClampAnisotropy(int configured)
{
    if (configured <= 1)
    {
        return {1u, false};
    }

    const auto requested = static_cast<std::uint32_t>(configured);
    if (requested > MaxSupportedAnisotropy)
    {
        return {MaxSupportedAnisotropy, true};
    }
    return {requested, false};
}

} // namespace Render::Textures
