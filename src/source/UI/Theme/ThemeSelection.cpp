#include "stdafx.h"

#include "UI/Theme/ThemeSelection.h"

#include "Core/Utilities/StringUtils.h"
#include "Data/GameConfig/GameConfig.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/ThemeReforge.h"

#include <utility>

namespace UI::Theme
{
bool Select(const std::string& name, Persistence persistence)
{
    if (!UI::RmlBridge::ThemeExists(name))
        return false;

    auto& config = GameConfig::GetInstance();
    config.SetRmlTheme(StringUtils::NarrowToWide(name));
    if (persistence == Persistence::Saved)
        config.Save();
    // The sprites whose [Reforge] preset changes are loaded again (ThemeReforge.h), released now so no
    // frame draws them with the old preset.
    auto previousReforge = UI::RmlBridge::ActiveReforgeList();
    UI::RmlBridge::SetActiveThemeName(name);
    UI::RmlBridge::QueueReforgeRelease(std::move(previousReforge));
    UI::RmlBridge::ReleaseQueuedReforgeTextures();
    UI::RmlBridge::ReloadAllThemedDocuments();
    return true;
}
} // namespace UI::Theme
