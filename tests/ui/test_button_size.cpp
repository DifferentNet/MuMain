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
constexpr float AttackUnits = 84.f;
constexpr float SkillUnits = 50.f;
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
    CHECK(AttackUnits * small >= 80.f);
}

TEST_CASE("a tablet's buttons grow only a little [ui][button-size]")
{
    const float tablet = ButtonSize::UnitCssPixels(TabletWidth, TabletHeight, 100);
    CHECK(tablet > 1.f);
    CHECK(tablet <= 1.15f + 1e-4f);
    // Held upright the short side decides the same.
    CHECK(ButtonSize::UnitCssPixels(TabletHeight, TabletWidth, 100) == doctest::Approx(tablet));
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
    CHECK(ButtonSize::UnitCssPixels(SmallPhoneWidth, SmallPhoneHeight, 130) < 1.1f);
}

TEST_CASE("an area not laid out yet gives one pixel [ui][button-size]")
{
    CHECK(ButtonSize::UnitCssPixels(0.f, 0.f, 100) == doctest::Approx(1.f));
    CHECK(ButtonSize::UnitCssPixels(SmallPhoneWidth, 0.f, 100) == doctest::Approx(1.f));
}
