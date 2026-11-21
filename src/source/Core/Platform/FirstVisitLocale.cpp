#include "Core/Platform/FirstVisitLocale.h"

#include <array>
#include <cctype>
#include <utility>

namespace Core::Platform
{
namespace
{
// The languages of src/Localization/Game.*.resx (Options -> Language) besides traditional Chinese.
constexpr std::array<std::string_view, 10> kGameLocales = {"en", "de", "es", "id", "ja", "pl", "pt", "ru", "tl", "uk"};

// Lower case, '_' as '-', without a weight ("ru;q=0.8") or spaces around.
std::string NormalTag(std::string_view tag)
{
    tag = tag.substr(0, tag.find(';'));
    const auto first = tag.find_first_not_of(' ');
    if (first == std::string_view::npos)
    {
        return {};
    }
    tag = tag.substr(first, tag.find_last_not_of(' ') - first + 1);
    std::string out(tag);
    for (char& c : out)
    {
        c = c == '_' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// True when tag is prefix itself or starts with prefix and a '-'.
bool HasPrefix(const std::string& tag, std::string_view prefix)
{
    return tag.compare(0, prefix.size(), prefix) == 0 && (tag.size() == prefix.size() || tag[prefix.size()] == '-');
}
} // namespace

std::string GameLocaleForTag(std::string_view tag)
{
    const std::string normal = NormalTag(tag);
    const std::string primary = normal.substr(0, normal.find('-'));
    if (primary == "zh")
    {
        // Taiwan, Hong Kong and Macau write the traditional script; zh-Hant names it outright.
        for (const std::string_view traditional : {"zh-tw", "zh-hk", "zh-mo", "zh-hant"})
        {
            if (HasPrefix(normal, traditional))
            {
                return "zh-TW";
            }
        }
        return {};
    }
    if (primary == "fil")
    {
        return "tl"; // Filipino: the game's texts are Tagalog
    }
    for (const std::string_view code : kGameLocales)
    {
        if (primary == code)
        {
            return std::string(code);
        }
    }
    return {};
}

FirstVisitLocale PickFirstVisitLocale(std::string_view siteLanguage, const std::vector<std::string>& browserLanguages)
{
    if (std::string code = GameLocaleForTag(siteLanguage); !code.empty())
    {
        return {std::move(code), true};
    }
    for (const std::string& tag : browserLanguages)
    {
        if (std::string code = GameLocaleForTag(tag); !code.empty())
        {
            return {std::move(code), false};
        }
    }
    return {};
}

bool SiteLanguageChanged(std::string_view siteLanguage, std::string_view lastSiteLanguage)
{
    const std::string site = NormalTag(siteLanguage);
    return !site.empty() && site != NormalTag(lastSiteLanguage);
}

bool SetConfigLocale(std::string& configText, std::string_view code)
{
    const auto section = configText.find("[UI]");
    if (section == std::string::npos)
    {
        return false;
    }
    const auto nextSection = configText.find("\n[", section + 1);
    auto key = configText.find("\nLocale=", section);
    if (key == std::string::npos || (nextSection != std::string::npos && key > nextSection))
    {
        return false;
    }
    const auto valueStart = key + 8; // after "\nLocale="
    auto valueEnd = configText.find_first_of("\r\n", valueStart);
    if (valueEnd == std::string::npos)
    {
        valueEnd = configText.size();
    }
    configText.replace(valueStart, valueEnd - valueStart, code);
    return true;
}
} // namespace Core::Platform
