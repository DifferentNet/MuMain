#include <doctest.h>

#include "Render/RmlUi/RmlTouchScroll.h"

#include <RmlUi/Core.h>

#include <cmath>
#include <memory>

using Render::RmlUi::RmlTouchScroll;

namespace
{
struct NullRenderer : Rml::RenderInterface
{
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override
    {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override
    {
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override
    {
        return 1;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

// A 100 px tall list of ten 40 px rows at (0, 0) with a 10 px scrollbar on its right, and a short list that
// fits at (200, 0). The second row is a button (`drag: block`, as base.rcss sets on every clickable element),
// the third a slider's part (`drag: drag`), the fourth an item grid's cell. `listStyle` is added to the long
// list's style.
Rml::String Markup(const char* listStyle)
{
    Rml::String rows;
    for (int row = 0; row < 10; ++row)
        rows += row == 1   ? "<div class='row' style='drag: block'/>"
                : row == 2 ? "<div class='row' style='drag: drag'/>"
                : row == 3 ? "<div class='item-grid row'/>"
                           : "<div class='row'/>";
    return "<rml><head><style>"
           ".row { display: block; height: 40px; }"
           "scrollbarvertical { width: 10px; }"
           "</style></head><body>"
           "<div id='list' style='position:absolute;left:0;top:0;width:100px;height:100px;overflow:auto;" +
           Rml::String(listStyle) + "'>" + rows +
           "</div>"
           "<div id='short' style='position:absolute;left:200px;top:0;width:100px;height:100px;overflow:auto'>"
           "<div class='row'/></div>"
           "</body></rml>";
}

class Fixture
{
public:
    explicit Fixture(const char* listStyle = "")
    {
        Rml::SetRenderInterface(&m_Renderer);
        REQUIRE(Rml::Initialise());
        context = Rml::CreateContext("touch-scroll", {400, 300});
        REQUIRE(context != nullptr);
        document = context->LoadDocumentFromMemory(Markup(listStyle));
        REQUIRE(document != nullptr);
        document->Show();
        // A frame drawn, as the game draws one: RmlUi works out the windows' transforms as it draws.
        context->Update();
        context->Render();
        list = document->GetElementById("list");
    }
    ~Fixture()
    {
        scroll.Reset();
        Rml::RemoveContext("touch-scroll");
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* list = nullptr;
    RmlTouchScroll scroll;

private:
    NullRenderer m_Renderer;
};
} // namespace

TEST_CASE("a finger dragged over a list scrolls it the other way, inside its range [ui][touch-scroll]")
{
    Fixture fixture;
    REQUIRE(fixture.scroll.Begin(fixture.context, {50.f, 50.f}));
    // Up by 30 px: the rows move up with the finger.
    fixture.scroll.Move({50.f, 50.f}, {50.f, 20.f});
    CHECK(fixture.list->GetScrollTop() == doctest::Approx(30.f));
    // Down by 80 px: back at the top, no further.
    fixture.scroll.Move({50.f, 20.f}, {50.f, 100.f});
    CHECK(fixture.list->GetScrollTop() == doctest::Approx(0.f));
    // A long way up: the list's end (400 - 100), no further.
    fixture.scroll.Move({50.f, 100.f}, {50.f, -900.f});
    CHECK(fixture.list->GetScrollTop() == doctest::Approx(300.f));
    // A list that does not scroll sideways keeps its place across.
    CHECK(fixture.list->GetScrollLeft() == doctest::Approx(0.f));
}

TEST_CASE("only a list with more than it shows, away from item cells, takes the finger [ui][touch-scroll]")
{
    Fixture fixture;
    // The list that fits, the empty screen, an item grid's cell (row 4: 120 to 160 px), the scrollbar
    // (dragged as a mouse drags it).
    CHECK_FALSE(fixture.scroll.Begin(fixture.context, {250.f, 20.f}));
    CHECK_FALSE(fixture.scroll.Begin(fixture.context, {350.f, 250.f}));
    CHECK_FALSE(fixture.scroll.Begin(fixture.context, {50.f, 130.f}));
    CHECK_FALSE(fixture.scroll.Begin(fixture.context, {95.f, 50.f}));
    // Nothing held: moves change nothing.
    fixture.scroll.Move({50.f, 50.f}, {50.f, 0.f});
    CHECK(fixture.list->GetScrollTop() == doctest::Approx(0.f));
}

TEST_CASE("a list's buttons scroll with the finger; a slider's parts keep it [ui][touch-scroll]")
{
    Fixture fixture;
    // The button's row (40 to 80 px): a tap there still clicks, a finger moving off it scrolls.
    CHECK(fixture.scroll.Begin(fixture.context, {50.f, 60.f}));
    // The slider's part (80 to 120 px) is dragged itself.
    CHECK_FALSE(fixture.scroll.Begin(fixture.context, {50.f, 90.f}));
}

TEST_CASE("a scaled window's list moves by the finger's distance on screen [ui][touch-scroll]")
{
    // Drawn at half size from its top left corner: 10 px on screen are 20 of the list's own.
    Fixture fixture("transform:scale(0.5);transform-origin:0 0");
    REQUIRE(fixture.scroll.Begin(fixture.context, {25.f, 25.f}));
    fixture.scroll.Move({25.f, 25.f}, {25.f, 15.f});
    CHECK(fixture.list->GetScrollTop() == doctest::Approx(20.f));
}

TEST_CASE("a flick glides on, a slow lift stops, and a finger stops a glide [ui][touch-scroll]")
{
    Fixture fixture;
    REQUIRE(fixture.scroll.Begin(fixture.context, {50.f, 50.f}));
    fixture.scroll.End({50.f, 50.f}, {0.f, -50.f});
    CHECK_FALSE(fixture.scroll.StopGlide());

    REQUIRE(fixture.scroll.Begin(fixture.context, {50.f, 50.f}));
    fixture.scroll.End({50.f, 50.f}, {0.f, -500.f});
    // Under way: a finger landing stops it where it is.
    CHECK(fixture.scroll.StopGlide());
    const float stopped = fixture.list->GetScrollTop();
    fixture.context->Update();
    fixture.context->Update();
    CHECK(fixture.list->GetScrollTop() == doctest::Approx(stopped));
    CHECK_FALSE(fixture.scroll.StopGlide());
}

TEST_CASE("a list that goes away while the finger holds it is let go [ui][touch-scroll]")
{
    Fixture fixture;
    REQUIRE(fixture.scroll.Begin(fixture.context, {50.f, 50.f}));
    fixture.context->UnloadDocument(fixture.document);
    fixture.context->Update();
    fixture.scroll.Move({50.f, 50.f}, {50.f, 0.f});
    fixture.scroll.End({50.f, 0.f}, {0.f, -500.f});
    CHECK_FALSE(fixture.scroll.StopGlide());
}
