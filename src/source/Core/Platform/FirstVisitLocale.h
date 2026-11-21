// The game's interface language on a browser's first visit to /play (no config.ini kept from earlier
// visits, Core/Platform/web/WebFileSystem.cpp): the language picked on the site when the game has it,
// else the first of the browser's languages the game has, else the English of config.ini.template.
// Later visits keep the player's config.ini, so a language chosen in Options stays, until the language
// picked on the site changes (SiteLanguageChanged): then the game picks again as on a first visit.
//
// Plain string work, so the rule is tested without a browser (tests/platform/test_first_visit_locale.cpp).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Core::Platform
{
struct FirstVisitLocale
{
    std::string code;      // the game's locale code ("ru", "zh-TW" ...); empty: none of them, keep English
    bool fromSite = false; // true: the site's choice (?lang=), false: the browser's languages
};

// The game's locale code for one language tag ("ru", "ru-RU", "pt_BR", "zh-Hant-TW"), or empty when the
// game has no texts in it. Chinese counts only in its traditional script, the one the game has.
[[nodiscard]] std::string GameLocaleForTag(std::string_view tag);

// siteLanguage: the language picked on the site (/play/?lang=, kept by the page), empty when none;
// browserLanguages: navigator.languages, most wanted first. An English site choice wins over the
// browser too: the code is then "en". withHanFonts false (the browser build) passes over Japanese and
// traditional Chinese: their font comes only after the first map there (BundledFonts.h
// IsLateFallbackFont), so the login would show boxes; Options still offers them.
[[nodiscard]] FirstVisitLocale PickFirstVisitLocale(std::string_view siteLanguage,
                                                    const std::vector<std::string>& browserLanguages,
                                                    bool withHanFonts = true);

// True when the language picked on the site (siteLanguage, as for PickFirstVisitLocale) is not the one the
// game saw on the last visit (lastSiteLanguage, kept by the page; empty before the first time). The game
// then picks its language again as on a first visit, so the site and the game speak the same language;
// while the site's stays the same, a language chosen in the game's Options stays. No site choice: false.
[[nodiscard]] bool SiteLanguageChanged(std::string_view siteLanguage, std::string_view lastSiteLanguage);

// Writes code into config.ini's [UI] Locale= line. False, text unchanged, when [UI] has no such line.
bool SetConfigLocale(std::string& configText, std::string_view code);
} // namespace Core::Platform
