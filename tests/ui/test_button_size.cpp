#include <doctest.h>

#include "UI/Mobile/ButtonSize.h"

namespace ButtonSize = UI::Mobile::ButtonSize;

namespace
{
// A phone held sideways, a bigger phone, a tablet (CSS pixels).
constexpr float SmallPhoneWidth = 740.f;
constexpr float SmallPhoneHeight = 360.f;
constexpr float BigPhoneWidth = 915.f;
constexpr float BigPhoneHeight = 412.f;
constexpr float TabletWidth = 1024.f;
constexpr float TabletHeight = 768.f;
// The attack button and a skill, in units (touch_controls.rcss).
constexpr float AttackUnits = 72.f;
constexpr float SkillUnits = 44.f;
// The phone HUD's top row at its smallest (fingers, the pad's glyphs) and the mini map's corner it keeps
// clear (ButtonSize.cpp).
constexpr float TopRowUnits = ButtonSize::TouchTopRowUnits;
constexpr float PadRowUnits = ButtonSize::PadTopRowUnits;
constexpr float MapCornerDp = 214.f;
} // namespace

TEST_CASE("the choices hold 100 % and a saved percent finds the nearest [ui][button-size]")
{
    CHECK(ButtonSize::Percents[ButtonSize::NearestIndex(100)] == 100);
    CHECK(ButtonSize::NearestIndex(0) == 0);
    CHECK(ButtonSize::NearestIndex(1000) == static_cast<int>(ButtonSize::Percents.size()) - 1);
    CHECK(ButtonSize::Percents[ButtonSize::NearestIndex(107)] == 100);
    CHECK(ButtonSize::Percents[ButtonSize::NearestIndex(112)] == 115);
}

TEST_CASE("a phone's buttons are a thumb wide whatever its size [ui][button-size]")
{
    const float small = ButtonSize::UnitCssPixels(SmallPhoneWidth, SmallPhoneHeight, 100);
    const float big = ButtonSize::UnitCssPixels(BigPhoneWidth, BigPhoneHeight, 100);
    CHECK(small == doctest::Approx(1.f));
    CHECK(big == doctest::Approx(1.f));
    // Apple's and Google's smallest touch target is 44-48 CSS pixels.
    CHECK(SkillUnits * small >= 44.f);
    CHECK(AttackUnits * small >= 70.f);
}

TEST_CASE("a tablet's buttons grow only a little [ui][button-size]")
{
    const float tablet = ButtonSize::UnitCssPixels(TabletWidth, TabletHeight, 100);
    CHECK(tablet > 1.f);
    CHECK(tablet <= 1.15f + 1e-4f);
    // Held upright the short side decides the same.
    CHECK(ButtonSize::UnitCssPixels(TabletHeight, TabletWidth, 100) == doctest::Approx(tablet));
    // In the phone's HUD, until its top row runs out of width.
    const float upright = ButtonSize::UnitCssPixels(TabletHeight, TabletWidth, 100, TopRowUnits);
    CHECK(upright > 1.f);
    CHECK(upright <= tablet);
    CHECK(upright == doctest::Approx((TabletHeight - MapCornerDp) / TopRowUnits));
}

TEST_CASE("a narrow phone's unit shrinks until the smallest top row fits beside the mini map [ui][button-size]")
{
    // An iPhone SE held sideways: 667 - 214 = 453 CSS pixels for a row of 524.6 units.
    const float se = ButtonSize::UnitCssPixels(667.f, 375.f, 100, TopRowUnits);
    CHECK(se == doctest::Approx((667.f - MapCornerDp) / TopRowUnits));
    CHECK(se < 1.f);
    CHECK(TopRowUnits * se + MapCornerDp <= 667.f + 1e-3f);
    // A bigger choice does not grow past the width either.
    CHECK(ButtonSize::UnitCssPixels(667.f, 375.f, 130, TopRowUnits) == doctest::Approx(se));
    // Below 100 % the row is drawn at its 100 % size: the cap shrinks with the percent.
    CHECK(ButtonSize::UnitCssPixels(667.f, 375.f, 60, TopRowUnits) == doctest::Approx(se * 0.6f));
    // The mini map's corner is in dp: two CSS pixels a dp leave less room.
    const float doubled = ButtonSize::UnitCssPixels(915.f, 412.f, 100, TopRowUnits, 2.f);
    CHECK(doubled == doctest::Approx((915.f - 2.f * MapCornerDp) / TopRowUnits));
    // The phones the controls are fitted to keep their full unit.
    CHECK(ButtonSize::UnitCssPixels(874.f, 402.f, 100, TopRowUnits) == doctest::Approx(1.f));
    CHECK(ButtonSize::UnitCssPixels(750.f, 381.f, 100, TopRowUnits) == doctest::Approx(1.f));
    // Outside the phone's HUD (a PC's touch screen or pad row, no row width given) nothing is capped.
    CHECK(ButtonSize::UnitCssPixels(667.f, 375.f, 100) == doctest::Approx(1.f));
}

TEST_CASE("with the pad's glyphs the top row needs 556.1 units [ui][button-size]")
{
    CHECK(PadRowUnits == doctest::Approx(278.f + 30.9f * 9.f));
    CHECK(PadRowUnits > TopRowUnits);
    // 750 - 214 = 536 CSS pixels: enough for the fingers' row, not for the pad's.
    const float pad = ButtonSize::UnitCssPixels(750.f, 381.f, 100, PadRowUnits);
    CHECK(pad == doctest::Approx((750.f - MapCornerDp) / PadRowUnits));
    CHECK(pad < 1.f);
    CHECK(ButtonSize::UnitCssPixels(874.f, 402.f, 100, PadRowUnits) == doctest::Approx(1.f));
    CHECK(ButtonSize::UnitCssPixels(915.f, 412.f, 100, PadRowUnits) == doctest::Approx(1.f));
}

TEST_CASE("the player's percent scales the unit until the screen's height runs out [ui][button-size]")
{
    CHECK(ButtonSize::UnitCssPixels(BigPhoneWidth, BigPhoneHeight, 80) == doctest::Approx(0.8f));
    const float bigger = ButtonSize::UnitCssPixels(BigPhoneWidth, BigPhoneHeight, 115);
    CHECK(bigger == doctest::Approx(1.15f));
    // 130 % would not fit a phone's height: it grows as far as it does.
    const float biggest = ButtonSize::UnitCssPixels(BigPhoneWidth, BigPhoneHeight, 130);
    CHECK(biggest > bigger);
    CHECK(biggest < 1.3f);
    CHECK(ButtonSize::UnitCssPixels(SmallPhoneWidth, SmallPhoneHeight, 130) < 1.15f);
    // The smallest choice.
    CHECK(ButtonSize::UnitCssPixels(BigPhoneWidth, BigPhoneHeight, 60) == doctest::Approx(0.6f));
}

TEST_CASE("an area not laid out yet gives one pixel [ui][button-size]")
{
    CHECK(ButtonSize::UnitCssPixels(0.f, 0.f, 100) == doctest::Approx(1.f));
    CHECK(ButtonSize::UnitCssPixels(SmallPhoneWidth, 0.f, 100) == doctest::Approx(1.f));
}
