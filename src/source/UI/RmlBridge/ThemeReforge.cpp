#include "stdafx.h"
#include "UI/RmlBridge/ThemeReforge.h"

#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/ThemeInheritance.h"

#include <RmlUi/Core/Core.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <optional>
#include <set>
#include <utility>

namespace UI::RmlBridge
{
namespace
{
using Render::Textures::Reforge::Preset;

constexpr const char* ThemesDirectory = "Data/Interface/RmlUi/themes";
constexpr const char* ReforgeSection = "Reforge";
constexpr std::string_view InterfaceRoot = "data/interface/";
constexpr std::string_view ThemeArt = "rmlui/themes/";
// The art CGlobalBitmap loads: the names a stylesheet writes and the containers on disk.
constexpr std::array<std::string_view, 4> SpriteExtensions = {".tga", ".jpg", ".ozt", ".ozj"};

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Where `root` starts in `path` as a whole directory name, or npos.
size_t FindRoot(const std::string& path, std::string_view root)
{
    for (size_t start = path.find(root); start != std::string::npos; start = path.find(root, start + 1))
    {
        if (start == 0 || path[start - 1] == '/')
            return start;
    }
    return std::string::npos;
}

// `name` without its sprite extension, or empty when it has none of them.
std::string WithoutSpriteExtension(const std::string& name)
{
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || name.find('/', dot) != std::string::npos)
        return {};
    const std::string_view extension = std::string_view(name).substr(dot);
    if (std::find(SpriteExtensions.begin(), SpriteExtensions.end(), extension) == SpriteExtensions.end())
        return {};
    return name.substr(0, dot);
}

Preset PresetOf(const ReforgeList& list, const std::string& key)
{
    const auto found = list.find(key);
    return found == list.end() ? Preset::None : found->second;
}

void ReportUnknownPreset(const std::string& key, const std::string& name)
{
    static std::set<std::string> reported;
    if (!reported.insert(name).second)
        return;
    g_ErrorReport.Write(L"> [RmlTheme] Unknown [Reforge] preset '%hs' (for '%hs'); the sprite is drawn as it is.\r\n",
                        name.c_str(), key.c_str());
}

std::optional<ReforgeList>& ActiveListStorage()
{
    static std::optional<ReforgeList> list;
    return list;
}

// The list from before a live switch whose textures are still to be released.
std::optional<ReforgeList>& QueuedReleaseStorage()
{
    static std::optional<ReforgeList> previous;
    return previous;
}
} // namespace

std::string NormalizeReforgeKey(std::string_view path)
{
    std::string text(path);
    std::replace(text.begin(), text.end(), '\\', '/');
    text = Lower(ThemePathUtf8(std::filesystem::u8path(text).lexically_normal()));
    const size_t root = FindRoot(text, InterfaceRoot);
    if (root == std::string::npos)
        return {};
    const std::string relative = text.substr(root + InterfaceRoot.size());
    if (relative.starts_with(ThemeArt))
        return {};
    return WithoutSpriteExtension(relative);
}

ReforgeList ReadReforgeList(const ThemeInheritance& theme)
{
    ReforgeList list;
    for (const auto& [key, setting] : theme.Section(ReforgeSection))
    {
        const auto preset = Render::Textures::Reforge::ParsePreset(Lower(setting.value));
        if (!preset)
            ReportUnknownPreset(key, setting.value);
        else if (*preset != Preset::None)
            list.emplace(key, *preset);
    }
    return list;
}

Preset LookupReforge(const ThemeInheritance& theme, std::string_view path)
{
    const std::string key = NormalizeReforgeKey(path);
    return key.empty() ? Preset::None : PresetOf(ReadReforgeList(theme), key);
}

const ReforgeList& ActiveReforgeList()
{
    auto& list = ActiveListStorage();
    if (!list)
        list = ReadReforgeList(ThemeInheritance(std::filesystem::path(ThemesDirectory) / GetActiveThemeName()));
    return *list;
}

Preset ActiveReforgePreset(std::string_view path)
{
    const std::string key = NormalizeReforgeKey(path);
    return key.empty() ? Preset::None : PresetOf(ActiveReforgeList(), key);
}

void InvalidateActiveReforge()
{
    ActiveListStorage().reset();
}

void QueueReforgeRelease(ReforgeList previous)
{
    // Two switches before a frame: RmlUi still holds the textures of the first list.
    auto& queued = QueuedReleaseStorage();
    if (!queued && previous != ActiveReforgeList())
        queued = std::move(previous);
}

void ReleaseQueuedReforgeTextures()
{
    auto& queued = QueuedReleaseStorage();
    if (!queued)
        return;
    const ReforgeList previous = std::move(*queued);
    queued.reset();
    const ReforgeList& current = ActiveReforgeList();
    for (const auto& source : Rml::GetTextureSourceList())
    {
        const std::string key = NormalizeReforgeKey(source);
        if (!key.empty() && PresetOf(previous, key) != PresetOf(current, key))
            Rml::ReleaseTexture(source);
    }
}
} // namespace UI::RmlBridge
