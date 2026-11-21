#include <doctest.h>

#include <string>
#include <vector>

#include "Core/Platform/FirstVisitLocale.h"

using Core::Platform::GameLocaleForTag;
using Core::Platform::PickFirstVisitLocale;
using Core::Platform::SetConfigLocale;
using Core::Platform::SiteLanguageChanged;

TEST_CASE("GameLocaleForTag maps browser tags to the game's languages")
{
    CHECK(GameLocaleForTag("ru") == "ru");
    CHECK(GameLocaleForTag("ru-RU") == "ru");
    CHECK(GameLocaleForTag("RU_md") == "ru");
    CHECK(GameLocaleForTag("ru;q=0.8") == "ru");
    CHECK(GameLocaleForTag(" uk-UA ") == "uk");
    CHECK(GameLocaleForTag("pt-BR") == "pt");
    CHECK(GameLocaleForTag("en-GB") == "en");
    CHECK(GameLocaleForTag("fil-PH") == "tl");
    CHECK(GameLocaleForTag("zh-TW") == "zh-TW");
    CHECK(GameLocaleForTag("zh-HK") == "zh-TW");
    CHECK(GameLocaleForTag("zh-Hant-CN") == "zh-TW");
    CHECK(GameLocaleForTag("zh-CN").empty());
    CHECK(GameLocaleForTag("zh").empty());
    CHECK(GameLocaleForTag("zh-twx").empty());
    CHECK(GameLocaleForTag("ro").empty());
    CHECK(GameLocaleForTag("ro-MD").empty());
    CHECK(GameLocaleForTag("").empty());
    CHECK(GameLocaleForTag("   ").empty());
    CHECK(GameLocaleForTag("r").empty());
}

TEST_CASE("PickFirstVisitLocale: the site's choice, then the browser, then none")
{
    const auto site = PickFirstVisitLocale("ru", {"en-US", "en"});
    CHECK(site.code == "ru");
    CHECK(site.fromSite);

    // An English site page chosen on purpose wins over a Russian browser.
    CHECK(PickFirstVisitLocale("en", {"ru-RU"}).code == "en");

    // Romanian on the site: the game has no Romanian texts, so the browser decides.
    const auto moldova = PickFirstVisitLocale("ro", {"ro-MD", "ro", "ru-RU", "en"});
    CHECK(moldova.code == "ru");
    CHECK_FALSE(moldova.fromSite);

    CHECK(PickFirstVisitLocale("", {"de-DE", "ru"}).code == "de");
    CHECK(PickFirstVisitLocale("", {"ro", "fr-FR"}).code.empty());
    CHECK(PickFirstVisitLocale("", {}).code.empty());
}

TEST_CASE("SiteLanguageChanged: a new choice on the site, not a missing one")
{
    CHECK(SiteLanguageChanged("en", "ru"));
    CHECK(SiteLanguageChanged("ro", "en"));
    // The first visit since the game began keeping it: the site's language counts as new.
    CHECK(SiteLanguageChanged("ru", ""));
    CHECK_FALSE(SiteLanguageChanged("ru", "ru"));
    CHECK_FALSE(SiteLanguageChanged("RU", "ru"));
    // No choice on the site: the game's own language stays.
    CHECK_FALSE(SiteLanguageChanged("", "ru"));
    CHECK_FALSE(SiteLanguageChanged("", ""));
}

TEST_CASE("SetConfigLocale rewrites only the [UI] Locale line")
{
    std::string crlf = "[LOGIN]\r\nLanguage=Eng\r\n\r\n[UI]\r\nLocale=en\r\nFont=\r\n";
    CHECK(SetConfigLocale(crlf, "ru"));
    CHECK(crlf == "[LOGIN]\r\nLanguage=Eng\r\n\r\n[UI]\r\nLocale=ru\r\nFont=\r\n");

    std::string last = "[UI]\nFont=\nLocale=en";
    CHECK(SetConfigLocale(last, "zh-TW"));
    CHECK(last == "[UI]\nFont=\nLocale=zh-TW");

    // A Locale= in another section is not the interface's.
    std::string other = "[UI]\nFont=\n[Other]\nLocale=en\n";
    CHECK_FALSE(SetConfigLocale(other, "ru"));
    CHECK(other == "[UI]\nFont=\n[Other]\nLocale=en\n");

    std::string none = "[LOGIN]\nLanguage=Eng\n";
    CHECK_FALSE(SetConfigLocale(none, "ru"));
}
