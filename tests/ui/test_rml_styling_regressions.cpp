#include "stdafx.h"
#include <doctest.h>

#include "RmlLayoutFixture.h"
#include "../../src/ThirdParty/RmlUi/Source/Core/DecoratorTiled.h"
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/TextShapingContext.h>

namespace
{
Rml::DecoratorTiled::Tile AtlasTile(Rml::DecoratorTiled::TileFitMode mode)
{
    Rml::DecoratorTiled::Tile tile;
    tile.fit_mode = mode;
    tile.size = {8.f, 6.f};
    tile.tile_data_calculated = true;
    tile.tile_data.size = tile.size;
    tile.tile_data.texcoords[0] = {0.25f, 0.25f};
    tile.tile_data.texcoords[1] = {0.5f, 0.5f};
    tile.align[0] = {Rml::Style::LengthPercentage::Percentage, 0.f};
    tile.align[1] = {Rml::Style::LengthPercentage::Percentage, 0.f};
    return tile;
}
}

TEST_CASE("atlas repetition clips partial tiles without sampling adjacent sprites [ui][styling]")
{
    UI::Tests::RmlLayoutFixture fixture;
    auto* document = fixture.LoadMarkup(R"(<rml><body><div id="panel"></div></body></rml>)");
    REQUIRE(document);
    auto* panel = document->GetElementById("panel");
    for (auto mode : {Rml::DecoratorTiled::REPEAT, Rml::DecoratorTiled::REPEAT_X, Rml::DecoratorTiled::REPEAT_Y})
    {
        CAPTURE(mode);
        Rml::Mesh mesh;
        auto tile = AtlasTile(mode);
        tile.GenerateGeometry(mesh, panel->GetComputedValues(), {}, {19.f, 14.f}, {8.f, 6.f});
        const size_t quads = mode == Rml::DecoratorTiled::REPEAT ? 9 : 3;
        CHECK(mesh.vertices.size() == quads * 4);
        CHECK(mesh.indices.size() == quads * 6);
        for (const auto& vertex : mesh.vertices)
        {
            CHECK(vertex.position.x >= 0.f);
            CHECK(vertex.position.x <= 19.f);
            CHECK(vertex.position.y >= 0.f);
            CHECK(vertex.position.y <= 14.f);
            CHECK(vertex.tex_coord.x >= 0.25f);
            CHECK(vertex.tex_coord.x <= 0.5f);
            CHECK(vertex.tex_coord.y >= 0.25f);
            CHECK(vertex.tex_coord.y <= 0.5f);
        }
        CHECK(mesh.vertices.back().position.y == (mode == Rml::DecoratorTiled::REPEAT_X ? 6.f : 14.f));
    }
}

TEST_CASE("fractional letter spacing accumulates consistently in measurement and drawing [ui][styling]")
{
    UI::Tests::RmlLayoutFixture fixture;
    auto* fonts = Rml::GetFontEngineInterface();
    const auto face = fonts->GetFontFaceHandle("liberation sans", Rml::Style::FontStyle::Normal,
                                              Rml::Style::FontWeight::Normal, 16);
    REQUIRE(face);
    const Rml::String text = "iiiiiiii";
    const Rml::String language = "en";
    Rml::TextShapingContext shaping{language};
    const int baseline = fonts->GetStringWidth(face, text, shaping);
    for (float spacing : {0.25f, -0.25f, 2.f})
    {
        CAPTURE(spacing);
        shaping.letter_spacing = spacing;
        const int measured = fonts->GetStringWidth(face, text, shaping);
        CHECK(measured == baseline + static_cast<int>(spacing * text.size()));
        Rml::TexturedMeshList meshes;
        const int drawn = fonts->GenerateString(fixture.context->GetRenderManager(), face, 0, text, {},
                                                Rml::ColourbPremultiplied(255, 255, 255, 255), 1.f, shaping, meshes);
        CHECK(drawn == measured);
    }
}

TEST_CASE("chat fill respects theme insets independently of modal backdrop styles [ui][styling]")
{
    UI::Tests::RmlLayoutFixture fixture;
    for (const std::string theme : {"legacy", "modern"})
    {
        CAPTURE(theme);
        auto* document = fixture.Load(theme, "chat_log");
        REQUIRE(document);
        auto* panel = document->GetElementById("panel");
        auto* fill = document->GetElementById("chat_backdrop");
        REQUIRE(panel);
        REQUIRE(fill);
        panel->SetClass("framed", true);
        panel->SetProperty("width", "281px");
        panel->SetProperty("height", "100px");
        fill->SetProperty("left", "10px");
        fill->SetProperty("right", "10px");
        fill->SetProperty("top", "5px");
        fill->SetProperty("bottom", "5px");
        fixture.Refresh();
        CHECK(fill->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(261.f));
        CHECK(fill->GetBox().GetSize(Rml::BoxArea::Border).y == doctest::Approx(90.f));
        document->Close();
    }
}

TEST_CASE("chat row probes carry a line's font and the room of a row's text in both themes [ui][styling]")
{
    UI::Tests::RmlLayoutFixture fixture;
    // The framed scrollbar's width per theme (chat_log.rcss), on a PC and on the phone's HUD: the probes' right
    // inset repeats it, so it is checked against the scrollbar RmlUi actually draws.
    struct ThemeScrollbar
    {
        std::string theme;
        bool phoneHud;
        float scrollbar;
    };
    for (const ThemeScrollbar& entry : {ThemeScrollbar{"legacy", false, 15.f}, ThemeScrollbar{"legacy", true, 24.f},
                                        ThemeScrollbar{"modern", false, 8.f}, ThemeScrollbar{"modern", true, 8.f}})
    {
        CAPTURE(entry.theme);
        CAPTURE(entry.phoneHud);
        fixture.context->ActivateTheme("mobile-hud", entry.phoneHud);
        auto* document = fixture.Load(entry.theme, "chat_log");
        REQUIRE(document);
        auto* panel = document->GetElementById("panel");
        auto* lines = document->GetElementById("lines");
        auto* probe = document->GetElementById("line_probe");
        auto* gmProbe = document->GetElementById("line_probe_gm");
        REQUIRE(panel);
        REQUIRE(lines);
        REQUIRE(probe);
        REQUIRE(gmProbe);
        panel->SetProperty("width", "281px");
        panel->SetProperty("height", "100px");
        lines->SetProperty("height", "90px");
        fixture.Refresh();

        // After the lines, so a line's place among the children stays its index in the model.
        REQUIRE(lines->GetNumChildren() >= 3);
        CHECK(lines->GetChild(lines->GetNumChildren() - 2) == probe);
        CHECK(lines->GetChild(lines->GetNumChildren() - 1) == gmProbe);
        const float unframedWidth = probe->GetBox().GetSize(Rml::BoxArea::Border).x;

        // More lines than the well shows, with the frame up: the scrollbar takes its width off the well.
        Rml::Element* filler = lines->InsertBefore(document->CreateElement("div"), probe);
        filler->SetProperty("display", "block");
        filler->SetProperty("height", "400px");
        filler->SetProperty("flex-shrink", "0");
        panel->SetClass("framed", true);
        fixture.Refresh();
        const float scrollbar = lines->GetBox().GetSize(Rml::BoxArea::Padding).x - lines->GetClientWidth();
        CHECK(scrollbar == doctest::Approx(entry.scrollbar));

        for (Rml::Element* row : {probe, gmProbe})
        {
            CHECK(row->GetFontFaceHandle() != 0);
            // No room taken, so the pointed-line test passes over them.
            CHECK(row->GetOffsetHeight() == 0.f);
            CHECK(row->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Bottom) == 0.f);
            // As wide as the well beside the scrollbar, whether the frame shows it or not.
            CHECK(row->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(lines->GetClientWidth()));
            CHECK(row->GetBox().GetSize(Rml::BoxArea::Border).x == doctest::Approx(unframedWidth));
            CHECK(row->GetBox().GetSize(Rml::BoxArea::Content).x > 0.f);
        }
        // A GM line is bold.
        CHECK(Rml::ElementUtilities::GetStringWidth(gmProbe, "Hello world, hello") >
              Rml::ElementUtilities::GetStringWidth(probe, "Hello world, hello"));
        document->Close();
    }
    fixture.context->ActivateTheme("mobile-hud", false);
}
