#include <doctest.h>

#include "UI/Mobile/PhoneHudFit.h"

#include <vector>

namespace Fit = UI::Mobile::PhoneHudFit;
using UI::Mobile::TouchTarget::Box;

namespace
{
// A shown control given by its right and bottom distances from a screen's corner, as the touch
// controls' theme places them (finger unit 1 = 1 px).
Box FromCorner(float screenWidth, float screenHeight, float right, float bottom, float width, float height)
{
    return Box{true, screenWidth - right - width, screenHeight - bottom - height, screenWidth - right,
               screenHeight - bottom};
}

// The controls round Attack on a screen with finger unit `u`: Attack, the five skills, Pick up,
// Auto and Skills, at their places in touch_controls.rcss.
std::vector<Box> ControlsAt(float W, float H, float u)
{
    const auto at = [W, H, u](float right, float bottom, float width, float height)
    { return FromCorner(W, H, right * u, bottom * u, width * u, height * u); };
    return {at(24, 24, 84, 84),   at(185, 56, 50, 50), at(168, 110, 50, 50), at(132, 154, 50, 50), at(82, 180, 50, 50),
            at(26, 185, 50, 50),  at(8, 124, 62, 44),  at(124, 8, 44, 44),   at(230, 118, 60, 30)};
}

std::vector<Box> ControlsAt640()
{
    return ControlsAt(640.f, 360.f, 1.f);
}

// The cards at the dock's scale `dock` under a top row 40 units tall that ends 72 units from the
// right edge, right of a 200 dp hero frame.
Fit::PartyArea AreaAt(float W, float H, float u, float dock)
{
    Fit::PartyArea area;
    area.gap = 4.f * u;
    area.right = W - 72.f * u;
    area.top = 40.f * u + area.gap;
    area.left = 204.f * dock + area.gap;
    area.floor = H;
    area.cardWidth = 77.f * dock;
    area.cardHeight = 23.f * dock;
    area.cardPitch = 24.f * dock;
    area.columnGap = 2.f * dock;
    return area;
}

Fit::PartyArea AreaAt640()
{
    return AreaAt(640.f, 360.f, 1.f, 0.75f);
}

// Whether the cards as fitted keep `gap` clear of every control.
bool ClearOfControls(const Fit::PartyArea& area, const Fit::PartyFit& fit, int cards, const std::vector<Box>& controls)
{
    const float width = area.cardWidth * fit.scale;
    for (int card = 0; card < cards; ++card)
    {
        const int column = card / fit.rows;
        const int row = card % fit.rows;
        const float right = area.right - column * (width + area.columnGap * fit.scale);
        const float top = area.top + row * area.cardPitch * fit.scale;
        const Box box{true, right - width, top, right, top + area.cardHeight * fit.scale};
        for (const Box& control : controls)
        {
            const bool apart = box.right + area.gap <= control.left || control.right + area.gap <= box.left ||
                               box.bottom + area.gap <= control.top || control.bottom + area.gap <= box.top;
            if (!apart)
                return false;
        }
    }
    return true;
}
} // namespace

TEST_CASE("the free bottom stops a gap above the first control under the span [ui][hud]")
{
    const std::vector<Box> boxes{{true, 100, 200, 150, 250}, {true, 300, 100, 350, 150}};
    CHECK(Fit::FreeBottom(boxes, 90, 160, 40, 4, 360) == doctest::Approx(196));
    CHECK(Fit::FreeBottom(boxes, 90, 400, 40, 4, 360) == doctest::Approx(96));
    // Nothing under the span: the floor.
    CHECK(Fit::FreeBottom(boxes, 400, 500, 40, 4, 360) == doctest::Approx(360));
    // A box above the top is no limit.
    CHECK(Fit::FreeBottom(boxes, 290, 360, 160, 4, 360) == doctest::Approx(360));
}

TEST_CASE("hidden controls take no room [ui][hud]")
{
    const std::vector<Box> boxes{{false, 100, 200, 150, 250}};
    CHECK(Fit::FreeBottom(boxes, 90, 160, 40, 4, 360) == doctest::Approx(360));
}

TEST_CASE("five cards fit at the biggest scale in two columns at 640x360 [ui][hud]")
{
    const std::vector<Box> controls = ControlsAt640();
    const Fit::PartyArea area = AreaAt640();
    const Fit::PartyFit fit = Fit::FitParty(area, 5, controls);
    CHECK(fit.fits);
    CHECK(fit.scale == doctest::Approx(Fit::MaxCardScale));
    CHECK(fit.columns == 2);
    CHECK(fit.rows >= 3);
    CHECK(ClearOfControls(area, fit, 5, controls));
}

TEST_CASE("a full party fits clear of the controls on the phone sizes the HUD is checked at [ui][hud]")
{
    struct Screen
    {
        float width, height, unit, dock;
    };
    // Finger unit and dock scale as the phone view gives them (TouchUnit, DockRightTransform).
    const Screen screens[] = {{568, 320, 0.928f, 0.667f}, {640, 360, 1.f, 0.75f},   {740, 360, 1.f, 0.75f},
                              {844, 390, 1.f, 0.8125f},   {915, 412, 1.f, 0.858f}, {821, 294, 0.852f, 0.6125f}};
    for (const Screen& screen : screens)
    {
        CAPTURE(screen.width);
        const std::vector<Box> controls = ControlsAt(screen.width, screen.height, screen.unit);
        const Fit::PartyArea area = AreaAt(screen.width, screen.height, screen.unit, screen.dock);
        const Fit::PartyFit fit = Fit::FitParty(area, 5, controls);
        CHECK(fit.fits);
        CHECK(fit.scale > 1.f);
        CHECK(ClearOfControls(area, fit, 5, controls));
    }
}

TEST_CASE("a small party stays in one column [ui][hud]")
{
    const std::vector<Box> controls = ControlsAt640();
    const Fit::PartyArea area = AreaAt640();
    const Fit::PartyFit fit = Fit::FitParty(area, 2, controls);
    CHECK(fit.fits);
    CHECK(fit.columns == 1);
    CHECK(fit.rows == 2);
    CHECK(fit.scale == doctest::Approx(Fit::MaxCardScale));
}

TEST_CASE("cards shrink before they would run into a control [ui][hud]")
{
    // One control under the column, 70 px below the top: three cards fit only once smaller.
    const std::vector<Box> controls{{true, 500, 114, 560, 140}};
    Fit::PartyArea area = AreaAt640();
    area.left = 500.f;
    const Fit::PartyFit fit = Fit::FitParty(area, 3, controls);
    CHECK(fit.fits);
    CHECK(fit.columns == 1);
    CHECK(fit.scale < Fit::MaxCardScale);
    CHECK(ClearOfControls(area, fit, 3, controls));
}

TEST_CASE("a party that never fits is laid out at the dock's scale in the most columns [ui][hud]")
{
    const std::vector<Box> controls{{true, 0, 50, 640, 60}};
    const Fit::PartyFit fit = Fit::FitParty(AreaAt640(), 5, controls);
    CHECK_FALSE(fit.fits);
    CHECK(fit.scale == doctest::Approx(1.f));
    CHECK(fit.columns == Fit::MaxColumns);
    CHECK(fit.rows == 3);
}

TEST_CASE("no cards, no fit [ui][hud]")
{
    const Fit::PartyFit fit = Fit::FitParty(AreaAt640(), 0, ControlsAt640());
    CHECK(fit.rows == 0);
    CHECK(fit.columns == 0);
}

TEST_CASE("the chat moves left until it clears the skill buttons beside it [ui][hud]")
{
    // The chat at 36 % of 640 px, 211 px wide, over the bottom 110 px: skill 0 is beside it.
    const std::vector<Box> controls = ControlsAt640();
    const float shift = Fit::ShiftLeft(controls, 230, 441, 250, 360, 4, 4);
    CHECK(shift == doctest::Approx(405 - 4 - 441));
}

TEST_CASE("a chat clear of the controls stays where it is [ui][hud]")
{
    const std::vector<Box> controls = ControlsAt640();
    CHECK(Fit::ShiftLeft(controls, 100, 300, 250, 360, 4, 4) == doctest::Approx(0));
}

TEST_CASE("the chat never moves past the screen's edge or a control on its left [ui][hud]")
{
    const std::vector<Box> wall{{true, 200, 250, 260, 360}};
    CHECK(Fit::ShiftLeft(wall, 100, 300, 250, 360, 4, 50) == doctest::Approx(-50));
    const std::vector<Box> both{{true, 0, 250, 80, 360}, {true, 250, 250, 300, 360}};
    CHECK(Fit::ShiftLeft(both, 100, 300, 250, 360, 4, 4) == doctest::Approx(-16));
}

TEST_CASE("a world label is under the HUD when it overlaps a shown zone [ui][hud]")
{
    const std::vector<Box> zones{{true, 0, 0, 640, 44}, {true, 4, 0, 154, 90}, {false, 300, 200, 400, 300}};
    // A balloon reaching up into the top row's band.
    CHECK(Fit::Overlaps(zones, {true, 300, 30, 380, 50}));
    // A name beside the hero frame, under the band.
    CHECK_FALSE(Fit::Overlaps(zones, {true, 160, 60, 220, 72}));
    // One that only touches the hero frame's edge.
    CHECK_FALSE(Fit::Overlaps(zones, {true, 154, 50, 200, 62}));
    // Under a hidden control.
    CHECK_FALSE(Fit::Overlaps(zones, {true, 320, 220, 360, 240}));
    // No zones: the PC's HUD.
    CHECK_FALSE(Fit::Overlaps({}, {true, 300, 30, 380, 50}));
}
