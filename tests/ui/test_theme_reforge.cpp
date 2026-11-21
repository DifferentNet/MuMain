#include <doctest.h>

#include "stdafx.h"
#include "UI/HUD/LiveGaugeMath.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/RmlBridge/ThemeFonts.h"
#include "UI/RmlBridge/ThemeInheritance.h"
#include "UI/RmlBridge/ThemeReforge.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/StyleSheetTypes.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

using Render::Textures::Reforge::ParsePreset;
using Render::Textures::Reforge::Preset;
using UI::RmlBridge::ActiveReforgeList;
using UI::RmlBridge::ActiveReforgePreset;
using UI::RmlBridge::LookupReforge;
using UI::RmlBridge::NormalizeReforgeKey;
using UI::RmlBridge::ReadReforgeList;
using UI::RmlBridge::ReforgeList;
using UI::RmlBridge::ThemeInheritance;

namespace
{
const Rml::Colourb White(0xff, 0xff, 0xff);
const Rml::Colourb Gold(0xff, 0xd2, 0x3c);
const Rml::Colourb StatYellow(0xe6, 0xe6, 0x00);

std::filesystem::path TemporaryRoot()
{
    return std::filesystem::temp_directory_path() /
           ("mu_theme_reforge_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void Write(const std::filesystem::path& path, const std::string& contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << contents;
}

// Theme folders in a temporary directory, for the tests that only read ini files.
struct ThemeFiles
{
    std::filesystem::path temporary = TemporaryRoot();
    std::filesystem::path root = temporary / "Data/Interface/RmlUi/themes";
    ~ThemeFiles()
    {
        std::filesystem::remove_all(temporary);
    }
    void Add(const std::string& path, const std::string& contents) const
    {
        Write(root / path, contents);
    }
};

// Hands out a handle per texture RmlUi loads and notes the ones it releases.
struct RecordingRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override
    {
        dimensions = {4, 4};
        loaded.push_back(source);
        return static_cast<Rml::TextureHandle>(loaded.size());
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 0;
    }
    void ReleaseTexture(Rml::TextureHandle handle) override
    {
        released.push_back(handle);
    }
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}

    std::vector<Rml::String> loaded;
    std::vector<Rml::TextureHandle> released;
};

// RmlUi with the game's file interface, run from a temporary directory that holds Data/Interface/RmlUi.
struct Fixture
{
    Fixture()
    {
        std::filesystem::create_directories(root);
        Rml::SetFileInterface(&files);
        Rml::SetRenderInterface(&renderer);
        REQUIRE(Rml::Initialise());
        UI::RmlBridge::ClearThemeFontCache();
        context = Rml::CreateContext("theme-reforge", {1024, 768});
        REQUIRE(context != nullptr);
        std::filesystem::current_path(root);
    }

    ~Fixture()
    {
        std::filesystem::current_path(previousDirectory);
        UI::RmlBridge::SetActiveThemeName(previousTheme);
        Rml::Shutdown();
        UI::RmlBridge::ClearThemeFontCache();
        Rml::SetFileInterface(nullptr);
        Rml::SetRenderInterface(nullptr);
        std::filesystem::remove_all(root);
    }

    std::filesystem::path Theme(const char* name) const
    {
        return root / "Data/Interface/RmlUi/themes" / name;
    }

    // The game's RmlUi files, themes included, as the client has them.
    void CopyRealInterface() const
    {
        std::filesystem::create_directories(root / "Data/Interface");
        std::filesystem::copy(MU_RMLUI_DIR, root / "Data/Interface/RmlUi", std::filesystem::copy_options::recursive);
        for (const char* name :
             {"LiberationSans-Regular.ttf", "LiberationSans-Bold.ttf", "DejaVuSans.ttf", "DejaVuSans-Bold.ttf"})
            REQUIRE(Rml::LoadFontFace((std::filesystem::path(MU_FONT_DIR) / name).generic_string()));
    }

    // `name` loaded against `theme` the way ThemedDocumentLoader does, without its data bindings.
    Rml::ElementDocument* LoadThemed(const char* theme, const char* name)
    {
        const std::string source = UI::RmlBridge::ThemedDocumentSourceUrl(name, theme);
        auto file = files.Open(source);
        if (!file)
            file = files.Open(std::string("Data/Interface/RmlUi/") + name);
        REQUIRE(file != 0);
        std::string markup(files.Length(file), '\0');
        files.Read(markup.data(), markup.size(), file);
        files.Close(file);
        const std::regex bindings(
            R"(\sdata-(?:model|for|if|class-[\w-]+|style-[\w-]+|attr-[\w-]+|event-[\w-]+)="[^"]*")");
        markup = std::regex_replace(markup, bindings, "");
        markup = std::regex_replace(markup, std::regex(R"(\{\{[^}]*\}\})"), "");
        auto* document = context->LoadDocumentFromMemory(markup, source);
        REQUIRE(document != nullptr);
        document->Show();
        context->Update();
        return document;
    }

    static Rml::Element* First(Rml::ElementDocument* document, const char* selector)
    {
        Rml::Element* element = document->QuerySelector(selector);
        REQUIRE(element != nullptr);
        return element;
    }

    std::filesystem::path root = TemporaryRoot();
    std::filesystem::path previousDirectory = std::filesystem::current_path();
    std::string previousTheme = UI::RmlBridge::GetActiveThemeName();
    UI::RmlBridge::ThemeFileInterface files;
    RecordingRenderer renderer;
    Rml::Context* context = nullptr;
};

// The [Reforge] key of every sprite under the client's Data/Interface, outside the RmlUi folder.
std::set<std::string> RealSpriteKeys()
{
    const std::filesystem::path interface = std::filesystem::path(MU_RMLUI_DIR).parent_path();
    std::set<std::string> keys;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(interface))
    {
        if (!entry.is_regular_file())
            continue;
        const auto relative = entry.path().lexically_relative(interface).generic_string();
        const std::string key = NormalizeReforgeKey("Data/Interface/" + relative);
        if (!key.empty())
            keys.insert(key);
    }
    return keys;
}
} // namespace

TEST_CASE("a reforge key is the sprite's path under Data/Interface without its extension [ui][rml-theme]")
{
    CHECK(NormalizeReforgeKey("Data/Interface/newui_item_box.tga") == "newui_item_box");
    CHECK(NormalizeReforgeKey("Data\\Interface\\PartCharge1\\newui_Menu03.OZJ") == "partcharge1/newui_menu03");
    CHECK(NormalizeReforgeKey("DATA/INTERFACE/newui_item_table01(L).OZT") == "newui_item_table01(l)");
    CHECK(NormalizeReforgeKey("Data/Interface/RmlUi/themes/legacy/../../../newui_item_back02-L.tga") ==
          "newui_item_back02-l");
    CHECK(NormalizeReforgeKey("Data/Interface/../Interface/newui_exbar.jpg") == "newui_exbar");
    CHECK(NormalizeReforgeKey("C:/Game/Data/Interface/newui_msgbox_back.jpg") == "newui_msgbox_back");
    CHECK(NormalizeReforgeKey("Data/Interface/newui_exit_00.ozt") == "newui_exit_00");

    // A theme's own art, the HD copies (the hook gets the original's path), other folders and other files.
    for (const char* path :
         {"Data/Interface/RmlUi/themes/skynet/images/gem.tga", "Data/HD/Interface/newui_item_box.OZT",
          "Data/Item/sword.tga", "newui_item_box.tga", "MyData/Interface/newui_item_box.tga",
          "Data/Interface/newui_item_box.png", "Data/Interface/newui_item_box", "", "Data/Interface/"})
    {
        CAPTURE(path);
        CHECK(NormalizeReforgeKey(path).empty());
    }
}

TEST_CASE("a child theme inherits its parent's reforge list and can turn entries off [ui][rml-theme]")
{
    ThemeFiles files;
    files.Add("parent/theme.ini",
              "[Reforge]\nnewui_item_box=cell\nnewui_exit_00=metal-flat\nPartCharge1/newui_menu03=Metal\n");
    files.Add("child/theme.ini", "[Theme]\nBase=parent\n[Reforge]\nnewui_exit_00=none\nnewui_msgbox_back=stone\n");
    files.Add("unknown/theme.ini", "[Theme]\nBase=child\n[Reforge]\nnewui_item_box=glow\n");
    // A [Reforge] section in tokens.ini is not read: only [Tokens] is.
    files.Add("child/tokens.ini", "[Reforge]\nnewui_item_money=metal\n");
    const ThemeInheritance parent(files.root / "parent");
    const ThemeInheritance child(files.root / "child");
    const ThemeInheritance unknown(files.root / "unknown");
    REQUIRE(child.IsValid());

    CHECK(ReadReforgeList(child) == ReforgeList{{"newui_item_box", Preset::Cell},
                                                {"partcharge1/newui_menu03", Preset::Metal},
                                                {"newui_msgbox_back", Preset::Stone}});
    CHECK(LookupReforge(parent, "Data/Interface/newui_exit_00.tga") == Preset::MetalFlat);
    CHECK(LookupReforge(child, "Data/Interface/newui_exit_00.tga") == Preset::None);
    CHECK(LookupReforge(child, "Data\\Interface\\partCharge1\\newui_menu03.jpg") == Preset::Metal);
    CHECK(LookupReforge(child, "Data/Interface/newui_item_money.tga") == Preset::None);
    CHECK(LookupReforge(child, "Data/HD/Interface/newui_item_box.tga") == Preset::None);
    // An unknown preset draws the sprite as it is.
    CHECK(LookupReforge(unknown, "Data/Interface/newui_item_box.tga") == Preset::None);
    CHECK(ReadReforgeList(unknown).count("newui_item_box") == 0);
    CHECK(ReadReforgeList(unknown).count("newui_msgbox_back") == 1);
}

TEST_CASE("legacy reforges nothing and skynet's list names real sprites [ui][rml-theme]")
{
    const std::filesystem::path themes = std::filesystem::path(MU_RMLUI_DIR) / "themes";
    for (const char* name : {"legacy", "modern"})
    {
        CAPTURE(name);
        const ThemeInheritance theme(themes / name);
        REQUIRE(theme.IsValid());
        CHECK(theme.Section("Reforge").empty());
        CHECK(ReadReforgeList(theme).empty());
    }

    const ThemeInheritance skynet(themes / "skynet");
    REQUIRE(skynet.IsValid());
    CHECK(skynet.Directories().size() == 2);
    REQUIRE(skynet.GetSetting("Meta", "DisplayName"));
    CHECK(skynet.GetSetting("Meta", "DisplayName")->value == "Skynet");
    REQUIRE(skynet.GetSetting("Capabilities", "NativeTextSize"));
    CHECK(skynet.GetSetting("Capabilities", "NativeTextSize")->value == "1");

    const auto entries = skynet.Section("Reforge");
    REQUIRE_FALSE(entries.empty());
    const std::set<std::string> sprites = RealSpriteKeys();
    REQUIRE_FALSE(sprites.empty());
    for (const auto& [key, setting] : entries)
    {
        CAPTURE(key);
        const auto preset = ParsePreset(setting.value);
        REQUIRE(preset);
        CHECK(*preset != Preset::None);
        CHECK(sprites.count(key) == 1);
    }
    const ReforgeList list = ReadReforgeList(skynet);
    CHECK(list.size() == entries.size());
    CHECK(list.at("newui_msgbox_back") == Preset::Stone);
    CHECK(list.at("newui_item_box") == Preset::Cell);
    // The green current-skill frame is in newui_menu03; newui_skillbox2 is orange.
    CHECK(list.at("partcharge1/newui_menu03") == Preset::Gild);
    CHECK(list.at("newui_skillbox2") == Preset::MetalFlat);
    // The HUD's menu buttons keep their light icons readable.
    for (const char* button :
         {"partcharge1/newui_menu_bt01", "partcharge1/newui_menu_bt02", "partcharge1/newui_menu_bt03",
          "partcharge1/newui_menu_bt04", "partcharge1/newui_menu_bt05"})
    {
        CAPTURE(button);
        CHECK(list.at(button) == Preset::MetalBright);
    }
    // The orbs, the level button and the item icons keep the original's look.
    for (const char* untouched : {"newui_menu_red", "newui_menu_blue", "newui_menu_green", "newui_menu_sd",
                                  "newui_menu_ag", "newui_chainfo_btn_level"})
    {
        CAPTURE(untouched);
        CHECK(list.count(untouched) == 0);
    }
}

TEST_CASE("a token with commas and parentheses reaches the stylesheet whole [ui][rml-theme]")
{
    Fixture fixture;
    Write(fixture.Theme("alpha") / "tokens.ini", "[Tokens]\nshade=rgba(18, 52, 86, 255)\n"
                                                 "shadow=0px 8px 18px rgba(0, 0, 0, 190)\n"
                                                 "glow=glow(0px 2px 0px 0px #ff9a3c80)\n");
    Write(fixture.Theme("alpha") / "paint.rcss",
          "#colored { color: token(shade); box-shadow: token(shadow); font-effect: token(glow); }");
    auto* document = fixture.context->LoadDocumentFromMemory(
        "<rml><head><link type='text/rcss' href='paint.rcss'/></head><body><div id='colored'>text</div></body></rml>",
        (fixture.Theme("alpha") / "view.rml").generic_string());
    REQUIRE(document != nullptr);
    document->Show();
    fixture.context->Update();
    const auto& values = Fixture::First(document, "#colored")->GetComputedValues();
    CHECK(values.color() == Rml::Colourb(18, 52, 86));
    CHECK(values.has_box_shadow());
    CHECK(values.has_font_effect());
}

TEST_CASE("skynet's tokens reach the inventory, the character window and the window shell [ui][rml-theme]")
{
    Fixture fixture;
    fixture.CopyRealInterface();

    // legacy draws as before: white titles without a glow, no window shadow, yellow stats.
    UI::RmlBridge::SetActiveThemeName("legacy");
    auto* inventory = fixture.LoadThemed("legacy", "my_inventory.rml");
    CHECK_FALSE(Fixture::First(inventory, "#panel")->GetComputedValues().has_box_shadow());
    CHECK(Fixture::First(inventory, ".title-text")->GetComputedValues().color() == White);
    CHECK_FALSE(Fixture::First(inventory, ".title-text")->GetComputedValues().has_font_effect());
    auto* character = fixture.LoadThemed("legacy", "character_info.rml");
    CHECK(Fixture::First(character, ".level-text")->GetComputedValues().color() == StatYellow);
    auto* shell = fixture.LoadThemed("legacy", "friend_shell.rml");
    CHECK(Fixture::First(shell, ".window-shell-header-title")->GetComputedValues().color() == White);
    CHECK_FALSE(Fixture::First(shell, ".window-shell-header-title")->GetComputedValues().has_font_effect());
    fixture.context->UnloadAllDocuments();
    fixture.context->Update();

    // skynet: gold titles with a glow, a shadow under the window, gold stats; the template's sheet too.
    UI::RmlBridge::SetActiveThemeName("skynet");
    inventory = fixture.LoadThemed("skynet", "my_inventory.rml");
    CHECK(Fixture::First(inventory, "#panel")->GetComputedValues().has_box_shadow());
    CHECK(Fixture::First(inventory, ".title-text")->GetComputedValues().color() == Gold);
    CHECK(Fixture::First(inventory, ".title-text")->GetComputedValues().has_font_effect());
    character = fixture.LoadThemed("skynet", "character_info.rml");
    CHECK(Fixture::First(character, ".level-text")->GetComputedValues().color() == Gold);
    shell = fixture.LoadThemed("skynet", "friend_shell.rml");
    CHECK(Fixture::First(shell, ".window-shell-header-title")->GetComputedValues().color() == Gold);
    CHECK(Fixture::First(shell, ".window-shell-header-title")->GetComputedValues().has_font_effect());
    fixture.context->UnloadAllDocuments();
    fixture.context->Update();
}

TEST_CASE("a live theme switch releases only the textures whose preset changed [ui][rml-theme]")
{
    Fixture fixture;
    Write(fixture.Theme("plain") / "theme.ini", "");
    Write(fixture.Theme("plain") / "paint.rcss", "div { display: block; width: 4px; height: 4px; }\n"
                                                 "#cells { decorator: image(../../../newui_item_box.tga); }\n"
                                                 "#exit { decorator: image(../../../newui_exit_00.tga); }\n");
    Write(fixture.Theme("forged") / "theme.ini", "[Theme]\nBase=plain\n[Reforge]\nnewui_item_box=cell\n");

    UI::RmlBridge::SetActiveThemeName("plain");
    CHECK(ActiveReforgeList().empty());
    auto* document = fixture.context->LoadDocumentFromMemory(
        "<rml><head><link type='text/rcss' href='paint.rcss'/></head><body><div id='cells'/><div id='exit'/></body>"
        "</rml>",
        (fixture.Theme("plain") / "view.rml").generic_string());
    REQUIRE(document != nullptr);
    document->Show();
    fixture.context->Update();
    fixture.context->Render();
    auto& renderer = fixture.renderer;
    REQUIRE(renderer.loaded.size() == 2);
    const auto cells = std::find_if(renderer.loaded.begin(), renderer.loaded.end(), [](const Rml::String& source)
                                    { return NormalizeReforgeKey(source) == "newui_item_box"; });
    REQUIRE(cells != renderer.loaded.end());
    const Rml::String cellsSource = *cells;
    const auto cellsHandle = static_cast<Rml::TextureHandle>(cells - renderer.loaded.begin() + 1);

    // To the forged theme: the cells' preset changes, the exit button's does not.
    ReforgeList previous = ActiveReforgeList();
    UI::RmlBridge::SetActiveThemeName("forged");
    CHECK(ActiveReforgePreset(cellsSource) == Preset::Cell);
    UI::RmlBridge::QueueReforgeRelease(std::move(previous));
    UI::RmlBridge::ReleaseQueuedReforgeTextures();
    CHECK(renderer.released == std::vector<Rml::TextureHandle>{cellsHandle});
    fixture.context->Update();
    fixture.context->Render();
    REQUIRE(renderer.loaded.size() == 3);
    CHECK(NormalizeReforgeKey(renderer.loaded.back()) == "newui_item_box");

    // The same list again: nothing to release, and the queue is empty after a frame.
    previous = ActiveReforgeList();
    UI::RmlBridge::SetActiveThemeName("forged");
    UI::RmlBridge::QueueReforgeRelease(std::move(previous));
    UI::RmlBridge::ReleaseQueuedReforgeTextures();
    UI::RmlBridge::ReleaseQueuedReforgeTextures();
    CHECK(renderer.released.size() == 1);

    // Back to plain: the reloaded cells go again.
    previous = ActiveReforgeList();
    UI::RmlBridge::SetActiveThemeName("plain");
    CHECK(ActiveReforgePreset(cellsSource) == Preset::None);
    UI::RmlBridge::QueueReforgeRelease(std::move(previous));
    UI::RmlBridge::ReleaseQueuedReforgeTextures();
    CHECK(renderer.released == std::vector<Rml::TextureHandle>{cellsHandle, 3});
    fixture.context->UnloadAllDocuments();
    fixture.context->Update();
}

namespace
{
// What skynet's tooltip.rml binds for a rich item tooltip (RmlTooltip.cpp), set by hand: a 12px text (1em), 8px side
// padding, a 1px frame, 14px and 15px rows, 200px wide (Measure()'s width).
constexpr float TooltipTextPx = 12.f;
constexpr float TooltipPaddingPx = 8.f;
constexpr float TooltipBorderPx = 1.f;
constexpr float TooltipWidthPx = 200.f;
constexpr float TooltipBoldLinePx = 15.f;
const Rml::Colourb TooltipEdge(0x7d, 0x64, 0x40);
const Rml::Colourb TooltipGold(0xe6, 0xb9, 0x5a);

std::string Px(float value)
{
    return std::to_string(value) + "px";
}

void BindRichPanel(Rml::Element& panel)
{
    panel.SetProperty("font-size", Px(TooltipTextPx));
    panel.SetProperty("border-width", Px(TooltipBorderPx));
    panel.SetProperty("padding-left", Px(TooltipPaddingPx));
    panel.SetProperty("padding-right", Px(TooltipPaddingPx));
    panel.SetProperty("width", Px(TooltipWidthPx));
    panel.SetProperty("--pad-px", Px(TooltipPaddingPx));
    panel.SetProperty("--border-px", Px(TooltipBorderPx));
    panel.SetProperty("--line-px", "14px");
    panel.SetProperty("--bold-line-px", Px(TooltipBoldLinePx));
}

// A line as the data-for template makes it: the line div, its text in a span.
Rml::Element* AddTooltipLine(Rml::ElementDocument& document, Rml::Element& panel, Rml::Element* before,
                             const std::string& classes, const std::string& text)
{
    Rml::Element* line = panel.InsertBefore(document.CreateElement("div"), before);
    line->SetClassNames(classes);
    line->SetInnerRML("<span class=\"tt-text\">" + text + "</span>");
    return line;
}

Rml::Vector2f BorderOffset(Rml::Element& element)
{
    return element.GetAbsoluteOffset(Rml::BoxArea::Border);
}

Rml::Vector2f BorderSize(Rml::Element& element)
{
    return element.GetBox().GetSize(Rml::BoxArea::Border);
}

std::string Token(const ThemeInheritance& theme, const char* name)
{
    const auto setting = theme.GetSetting("Tokens", name);
    REQUIRE(setting);
    return setting->value;
}
} // namespace

TEST_CASE("only skynet declares the rich item tooltip and the rarity cells [ui][rml-theme]")
{
    Fixture fixture;
    fixture.CopyRealInterface();
    for (const char* name : {"legacy", "modern"})
    {
        CAPTURE(name);
        UI::RmlBridge::SetActiveThemeName(name);
        CHECK_FALSE(UI::RmlBridge::ThemeHasRichItemTooltip());
        CHECK_FALSE(UI::RmlBridge::ThemeHasItemRarityCells());
    }
    UI::RmlBridge::SetActiveThemeName("skynet");
    CHECK(UI::RmlBridge::ThemeHasRichItemTooltip());
    CHECK(UI::RmlBridge::ThemeHasItemRarityCells());
    CHECK(UI::RmlBridge::ThemeUsesNativeTextSize());
    // Back to legacy: the switch recomputes both.
    UI::RmlBridge::SetActiveThemeName("legacy");
    CHECK_FALSE(UI::RmlBridge::ThemeHasRichItemTooltip());
    CHECK_FALSE(UI::RmlBridge::ThemeHasItemRarityCells());
}

TEST_CASE("only skynet declares the live HUD, and its [LiveHud] values read whole [ui][rml-theme]")
{
    Fixture fixture;
    fixture.CopyRealInterface();
    for (const char* name : {"legacy", "modern"})
    {
        CAPTURE(name);
        UI::RmlBridge::SetActiveThemeName(name);
        CHECK_FALSE(UI::RmlBridge::ThemeHasLiveHud());
    }
    UI::RmlBridge::SetActiveThemeName("skynet");
    CHECK(UI::RmlBridge::ThemeHasLiveHud());
    UI::RmlBridge::SetActiveThemeName("legacy");
    CHECK_FALSE(UI::RmlBridge::ThemeHasLiveHud());

    // Every key is set, none with a comment after its value: ThemeInheritance keeps one, and the value is then
    // refused for the default.
    std::map<std::string, std::string> values;
    for (const auto& [key, setting] : ThemeInheritance(fixture.Theme("skynet")).Section("LiveHud"))
    {
        CAPTURE(key);
        CHECK(setting.value.find(';') == std::string::npos);
        values.emplace(key, setting.value);
    }
    for (const char* key : {"gemoutline", "gemzoom", "emptyshade", "waveamplitude", "waveperiodlife", "waveperiodmana",
                            "arcstart", "arcsweep", "arcwidth", "lowlife", "pulseperiod", "pulsestrength", "halolife",
                            "halomana", "halopoison", "sweepshade", "rimouter", "riminner", "rimwidth"})
    {
        CAPTURE(key);
        CHECK(values.count(key) == 1);
    }
    const UI::HUD::LiveGauge::LiveHudConfig config = UI::HUD::LiveGauge::ReadLiveHudConfig(values);
    CHECK(config.gemOutline.size() == 8);
    CHECK(config.gemZoom == doctest::Approx(1.32f));
    CHECK(config.wavePeriodLife == doctest::Approx(2.6f));
    CHECK(config.wavePeriodMana == doctest::Approx(3.3f));
    CHECK(config.haloLife == 0xFF1834FFu);
    CHECK(config.sweepShade == 0xCC030406u);
    CHECK(config.rimOuter == 0xFF2A465Cu);
    CHECK(config.rimInner == 0xFF07090Bu);
    CHECK(config.rimWidth == doctest::Approx(1.5f));
    CHECK(ThemeInheritance(fixture.Theme("legacy")).Section("LiveHud").empty());
}

TEST_CASE("legacy's rarity tokens are neutral and skynet's are its own [ui][rml-theme]")
{
    const std::filesystem::path themes = std::filesystem::path(MU_RMLUI_DIR) / "themes";
    const ThemeInheritance legacy(themes / "legacy");
    REQUIRE(legacy.IsValid());
    const std::string normal = Token(legacy, "cell-tint-normal");
    for (const char* tint : {"cell-tint-exc", "cell-tint-anc", "cell-tint-socket", "cell-tint-gold"})
    {
        CAPTURE(tint);
        CHECK(Token(legacy, tint) == normal);
    }
    CHECK(Token(legacy, "cell-frame-width") == "0px");
    CHECK(Token(legacy, "cell-frame-gold-width") == "0px");
    for (const char* frame :
         {"cell-frame-fine", "cell-frame-exc", "cell-frame-anc", "cell-frame-socket", "cell-frame-gold"})
    {
        CAPTURE(frame);
        CHECK(Token(legacy, frame) == "#00000000");
    }

    const ThemeInheritance skynet(themes / "skynet");
    REQUIRE(skynet.IsValid());
    CHECK(Token(skynet, "cell-frame-width") == "1px");
    // +13..+15's gold frame is drawn wider than the others, so it reads at a glance.
    CHECK(Token(skynet, "cell-frame-gold-width") == "2px");
    CHECK(Token(skynet, "cell-frame-gold") == "#e6b95a");
    CHECK(Token(skynet, "tt-gold") == "#e6b95a");
    CHECK(Token(skynet, "tooltip-edge") == "#7d6440");
    // The tooltip's own document comes from skynet, with its rich sheet beside it.
    const auto document = skynet.ResolveFile("tooltip.rml");
    REQUIRE(document);
    CHECK(document->parent_path().filename() == "skynet");
    CHECK(skynet.ResolveFile("tooltip_rich.rcss"));
    CHECK_FALSE(legacy.ResolveFile("tooltip_rich.rcss"));
}

TEST_CASE("skynet's tooltip is warm and framed in bronze, legacy's stays black [ui][rml-theme]")
{
    Fixture fixture;
    fixture.CopyRealInterface();

    UI::RmlBridge::SetActiveThemeName("legacy");
    auto* legacy = fixture.LoadThemed("legacy", "tooltip.rml");
    Rml::Element* panel = Fixture::First(legacy, "#tooltip_panel");
    CHECK(panel->GetComputedValues().background_color() == Rml::Colourb(0, 0, 0, 204));
    CHECK(panel->GetComputedValues().border_top_color() == Rml::Colourb(0, 0, 0));
    CHECK(legacy->QuerySelector("#tt_gold") == nullptr);
    fixture.context->UnloadAllDocuments();
    fixture.context->Update();

    UI::RmlBridge::SetActiveThemeName("skynet");
    auto* skynet = fixture.LoadThemed("skynet", "tooltip.rml");
    panel = Fixture::First(skynet, "#tooltip_panel");
    CHECK(panel->GetComputedValues().background_color() == Rml::Colourb(12, 9, 7, 250));
    CHECK(panel->GetComputedValues().border_top_color() == TooltipEdge);
    // The line template stays the panel's first child; the frame and the diamonds follow it, hidden unless rich.
    REQUIRE(panel->GetNumChildren() > 0);
    CHECK(panel->GetChild(0)->IsClassSet("tt-line"));
    CHECK(Fixture::First(skynet, "#tt_gold")->GetComputedValues().display() == Rml::Style::Display::None);
    CHECK(Fixture::First(skynet, ".tt-k")->GetComputedValues().display() == Rml::Style::Display::None);
    CHECK_FALSE(panel->GetComputedValues().has_box_shadow());
    fixture.context->UnloadAllDocuments();
    fixture.context->Update();
}

TEST_CASE("skynet's rich tooltip lays out its header, dividers, pill, diamonds and gold frame [ui][rml-theme]")
{
    Fixture fixture;
    fixture.CopyRealInterface();
    UI::RmlBridge::SetActiveThemeName("skynet");
    auto* document = fixture.LoadThemed("skynet", "tooltip.rml");
    Rml::Element* panel = Fixture::First(document, "#tooltip_panel");
    panel->SetClassNames("tt-centered tt-rich rar-anc up-gold");
    BindRichPanel(*panel);
    Rml::Element* gold = Fixture::First(document, "#tt_gold");

    // The template is the name line; the rest as ArrangeRich() orders them.
    Rml::Element* name = panel->GetChild(0);
    name->SetClassNames("tt-line bold tt-hl-greenblue tt-name tt-head");
    name->SetInnerRML("<span class=\"tt-text\">Ancient Breastplate +15</span>");
    name->SetProperty("font-size", Px(TooltipTextPx * 1.3f));
    Rml::Element* kind = AddTooltipLine(*document, *panel, gold, "tt-line tt-kind tt-head", "Ancient - Armour");
    Rml::Element* cp =
        AddTooltipLine(*document, *panel, gold, "tt-line tt-redpurple tt-cp tt-head tt-head-last", "Combat Power 1234");
    Rml::Element* headSpacer = AddTooltipLine(*document, *panel, gold, "tt-line tt-half-spacer", "");
    AddTooltipLine(*document, *panel, gold, "tt-line", "Defense: 120");
    Rml::Element* divider = AddTooltipLine(*document, *panel, gold, "tt-line tt-half-spacer", "");
    Rml::Element* section = AddTooltipLine(*document, *panel, gold, "tt-line tt-yellow tt-section", "Set option");
    Rml::Element* hint = AddTooltipLine(*document, *panel, gold, "tt-line tt-gray tt-hint", "Hold Shift for details");
    fixture.context->Update();

    const Rml::Vector2f panelOffset = BorderOffset(*panel);
    const Rml::Vector2f panelSize = BorderSize(*panel);
    REQUIRE(panelSize.x == doctest::Approx(TooltipWidthPx + 2 * (TooltipPaddingPx + TooltipBorderPx)));

    SUBCASE("the header bars run across the whole box in the rarity's colour")
    {
        for (Rml::Element* line : {name, kind, cp})
        {
            CHECK(BorderSize(*line).x == doctest::Approx(TooltipWidthPx + 2 * TooltipPaddingPx));
            CHECK(BorderOffset(*line).x == doctest::Approx(panelOffset.x + TooltipBorderPx));
            CHECK(line->GetComputedValues().background_color() == Rml::Colourb(0x2b, 0x3d, 0xb6));
        }
        // The ancient name keeps its green; the bar replaces its blue highlight.
        CHECK(name->GetComputedValues().color() == Rml::Colourb(0x00, 0xff, 0x00));
        CHECK(BorderSize(*name).y == doctest::Approx(TooltipBoldLinePx * 1.6f));
        CHECK(kind->GetComputedValues().color() == Rml::Colourb(0xcb, 0xbf, 0xa9));
        CHECK(cp->GetComputedValues().border_bottom_width() == doctest::Approx(TooltipBorderPx));
        CHECK(cp->GetComputedValues().border_bottom_color() == TooltipEdge);
    }

    SUBCASE("a divider is a one-pixel gradient, none under the header")
    {
        CHECK(BorderSize(*headSpacer).y == doctest::Approx(0.f));
        CHECK_FALSE(headSpacer->GetComputedValues().has_decorator());
        CHECK(BorderSize(*divider).y == doctest::Approx(1.f));
        CHECK(BorderSize(*divider).x == doctest::Approx(TooltipWidthPx - 2 * 1.2f * TooltipTextPx));
        REQUIRE(divider->GetComputedValues().has_decorator());
        const Rml::Property* decorator = divider->GetProperty("decorator");
        REQUIRE(decorator != nullptr);
        const auto declarations = decorator->Get<Rml::DecoratorsPtr>();
        REQUIRE(declarations);
        REQUIRE(declarations->list.size() == 1);
        CHECK(declarations->list.front().type == "linear-gradient");
        CHECK(section->GetComputedValues().font_weight() == Rml::Style::FontWeight::Bold);
        CHECK(hint->GetComputedValues().color() == Rml::Colourb(0x8d, 0x8d, 0x8d));
    }

    SUBCASE("the Combat Power pill holds its text and its own padding")
    {
        Rml::Element* pill = cp->GetFirstChild();
        REQUIRE(pill != nullptr);
        CHECK(pill->GetComputedValues().display() == Rml::Style::Display::InlineBlock);
        const Rml::Box& box = pill->GetBox();
        CHECK(box.GetEdge(Rml::BoxArea::Padding, Rml::BoxEdge::Left) == doctest::Approx(0.8f * TooltipTextPx));
        CHECK(box.GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Right) == doctest::Approx(1.f));
        const float text = static_cast<float>(Rml::ElementUtilities::GetStringWidth(pill, "Combat Power 1234"));
        CHECK(box.GetSize(Rml::BoxArea::Content).x >= text - 1.f);
        // One row: the pill does not wrap its text.
        CHECK(box.GetSize(Rml::BoxArea::Content).y < 2 * TooltipBoldLinePx);
    }

    SUBCASE("the diamonds sit on the frame's corners")
    {
        const float half = 0.25f * TooltipTextPx;
        const std::array<Rml::Element*, 2> diamonds = {Fixture::First(document, ".tt-k1"),
                                                       Fixture::First(document, ".tt-k4")};
        for (Rml::Element* diamond : diamonds)
        {
            CHECK(diamond->GetComputedValues().display() == Rml::Style::Display::Block);
            CHECK(BorderSize(*diamond).x == doctest::Approx(2 * half));
            CHECK(diamond->GetComputedValues().background_color() == TooltipGold);
        }
        // Centred on the middle of the 1px frame line.
        const Rml::Vector2f first = BorderOffset(*diamonds[0]);
        CHECK(first.x + half == doctest::Approx(panelOffset.x + TooltipBorderPx * 0.5f));
        CHECK(first.y + half == doctest::Approx(panelOffset.y + TooltipBorderPx * 0.5f));
        const Rml::Vector2f last = BorderOffset(*diamonds[1]);
        CHECK(last.x + half == doctest::Approx(panelOffset.x + panelSize.x - TooltipBorderPx * 0.5f));
        CHECK(last.y + half == doctest::Approx(panelOffset.y + panelSize.y - TooltipBorderPx * 0.5f));
    }

    SUBCASE("+13..+15 frames the box in gold just outside, and only the frame's opacity moves")
    {
        CHECK(panel->GetComputedValues().border_top_color() == TooltipGold);
        CHECK(panel->GetComputedValues().has_box_shadow());
        REQUIRE(gold->GetComputedValues().display() == Rml::Style::Display::Block);
        const float outside = 0.25f * TooltipTextPx;
        CHECK(BorderOffset(*gold).x == doctest::Approx(panelOffset.x - outside));
        CHECK(BorderOffset(*gold).y == doctest::Approx(panelOffset.y - outside));
        CHECK(BorderSize(*gold).x == doctest::Approx(panelSize.x + 2 * outside));
        CHECK(BorderSize(*gold).y == doctest::Approx(panelSize.y + 2 * outside));
        CHECK(gold->GetComputedValues().has_box_shadow());
        CHECK(gold->GetComputedValues().animation() == nullptr);

        Rml::Element* pulse = Fixture::First(document, "#tt_gold_pulse");
        CHECK(BorderSize(*pulse).x == doctest::Approx(BorderSize(*gold).x));
        CHECK(pulse->GetComputedValues().border_top_width() > 0.f);
        CHECK_FALSE(pulse->GetComputedValues().has_box_shadow());
        const Rml::AnimationList* animation = pulse->GetComputedValues().animation();
        REQUIRE(animation != nullptr);
        CHECK(animation->size() == 1);
        // The pulse starts at its dimmest.
        CHECK(pulse->GetComputedValues().opacity() < 0.9f);
    }

    SUBCASE("too tall for the window even fitted: the frame stays, the shadow and the glow go")
    {
        panel->SetClassNames("tt-centered tt-rich rar-anc up-gold tt-tall");
        fixture.context->Update();
        CHECK_FALSE(panel->GetComputedValues().has_box_shadow());
        CHECK(panel->GetComputedValues().border_top_color() == TooltipGold);
        REQUIRE(gold->GetComputedValues().display() == Rml::Style::Display::Block);
        CHECK_FALSE(gold->GetComputedValues().has_box_shadow());
        CHECK(Fixture::First(document, "#tt_gold_pulse")->GetComputedValues().border_top_width() > 0.f);
    }

    // Without the band: no gold frame, the bronze frame again.
    panel->SetClassNames("tt-centered tt-rich rar-anc");
    fixture.context->Update();
    CHECK(gold->GetComputedValues().display() == Rml::Style::Display::None);
    CHECK(panel->GetComputedValues().border_top_color() == TooltipEdge);
    fixture.context->UnloadAllDocuments();
    fixture.context->Update();
}

TEST_CASE("the live gems show only in skynet, round its gauges, and never on the phone's HUD [ui][rml-theme]")
{
    Fixture fixture;
    fixture.CopyRealInterface();
    const auto display = [](Rml::ElementDocument* document, const char* selector)
    { return Fixture::First(document, selector)->GetComputedValues().display(); };
    using Rml::Style::Display;

    SUBCASE("legacy and modern hide them and keep their own HUD")
    {
        for (const char* name : {"legacy", "modern"})
        {
            CAPTURE(name);
            UI::RmlBridge::SetActiveThemeName(name);
            auto* document = fixture.LoadThemed(name, "main_frame.rml");
            CHECK(display(document, "#orb_left") == Display::None);
            CHECK(display(document, "#orb_right") == Display::None);
            if (std::string(name) == "legacy")
            {
                CHECK(BorderSize(*Fixture::First(document, "#hud_layout")).y == doctest::Approx(51.f));
                CHECK(display(document, "#bar_hp_fill") != Display::None);
                CHECK(BorderSize(*Fixture::First(document, "#bar_hp")).x == doctest::Approx(45.f));
            }
            document->Close();
        }
    }

    SUBCASE("skynet sets each gem's rect where its gauge's hint is, in a ring over the strip")
    {
        UI::RmlBridge::SetActiveThemeName("skynet");
        auto* document = fixture.LoadThemed("skynet", "main_frame.rml");
        CHECK(BorderSize(*Fixture::First(document, "#hud_layout")).y == doctest::Approx(64.f));
        for (const auto& [orb, gem, bar, body] :
             {std::array<const char*, 4>{"#orb_left", "#orb_hp_box", "#bar_hp", "#orb_hp_body"},
              std::array<const char*, 4>{"#orb_right", "#orb_mp_box", "#bar_mp", "#orb_mp_body"}})
        {
            CAPTURE(orb);
            CHECK(display(document, orb) != Display::None);
            Rml::Element* ring = Fixture::First(document, orb);
            CHECK(BorderSize(*ring).x == doctest::Approx(62.f));
            CHECK(BorderSize(*ring).y == doctest::Approx(62.f));
            // The meshes fill the ring.
            CHECK(BorderOffset(*Fixture::First(document, body)).x == doctest::Approx(BorderOffset(*ring).x));
            CHECK(BorderSize(*Fixture::First(document, body)).x == doctest::Approx(62.f));
            Rml::Element* rect = Fixture::First(document, gem);
            Rml::Element* gauge = Fixture::First(document, bar);
            CHECK(BorderSize(*rect).x == doctest::Approx(54.f));
            CHECK(BorderSize(*rect).y == doctest::Approx(46.8f));
            CHECK(BorderOffset(*rect).x == doctest::Approx(BorderOffset(*gauge).x));
            CHECK(BorderOffset(*rect).y == doctest::Approx(BorderOffset(*gauge).y));
            CHECK(BorderSize(*gauge).x == doctest::Approx(54.f));
            CHECK(BorderSize(*gauge).y == doctest::Approx(46.8f));
        }
        // The meshes replace the fills.
        CHECK(display(document, "#bar_hp_fill") == Display::None);
        // The plate and the gem's rect never take the pointer from what is under them.
        CHECK(Fixture::First(document, "#orb_left .orb-plate")->GetComputedValues().pointer_events() ==
              Rml::Style::PointerEvents::None);
        CHECK(Fixture::First(document, "#orb_hp_box")->GetComputedValues().pointer_events() ==
              Rml::Style::PointerEvents::None);

        // The number is centred on the gem (its translateX(-50%) takes half its width back), 3dp under legacy's
        // height, outlined, with nothing behind it.
        Rml::Element* number = Fixture::First(document, "#bar_hp .gauge-num.value-current");
        number->SetInnerRML("1234");
        fixture.context->Update();
        const auto& style = number->GetComputedValues();
        CHECK(style.background_color().alpha == 0);
        CHECK(style.border_top_width() == doctest::Approx(0.f));
        CHECK(style.has_font_effect());
        Rml::Element* gauge = Fixture::First(document, "#bar_hp");
        CHECK(BorderOffset(*number).y - gauge->GetAbsoluteOffset(Rml::BoxArea::Padding).y == doctest::Approx(40.8f));
        const float gemCentre =
            gauge->GetAbsoluteOffset(Rml::BoxArea::Padding).x + gauge->GetBox().GetSize(Rml::BoxArea::Padding).x / 2.f;
        CHECK(BorderOffset(*number).x == doctest::Approx(gemCentre));
        CHECK(BorderSize(*number).x < 54.f);
        CHECK(Fixture::First(document, "#skill_list")->GetComputedValues().bottom().value == doctest::Approx(65.f));
        document->Close();
    }

    SUBCASE("the phone's HUD has no gems and no room for the bar")
    {
        UI::RmlBridge::SetActiveThemeName("skynet");
        fixture.context->ActivateTheme("mobile-hud", true);
        auto* document = fixture.LoadThemed("skynet", "main_frame.rml");
        CHECK(BorderSize(*Fixture::First(document, "#hud_layout")).y == doctest::Approx(0.f));
        CHECK(display(document, "#orb_left") == Display::None);
        CHECK(display(document, "#orb_right") == Display::None);
        CHECK(Fixture::First(document, "#skill_list")->GetComputedValues().bottom().value == doctest::Approx(118.f));
        document->Close();
        fixture.context->ActivateTheme("mobile-hud", false);
    }
}
