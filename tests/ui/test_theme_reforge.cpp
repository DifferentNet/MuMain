#include <doctest.h>

#include "stdafx.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/ThemeFileInterface.h"
#include "UI/RmlBridge/ThemeFonts.h"
#include "UI/RmlBridge/ThemeInheritance.h"
#include "UI/RmlBridge/ThemeReforge.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ComputedValues.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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
