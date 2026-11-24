#include "doctest.h"

#include "Render/Effects/EchoTrail.h"

#include <limits>

using namespace Render::Effects::EchoTrail;

namespace
{
// Walks a trail along x, `step` units a frame, one frame every `frameMs`.
struct Walker
{
    Trail trail;
    Settings settings = PcSettings();
    float x = 0.f;
    double now = 0.0;

    int Step(float step, double frameMs = 16.0)
    {
        x += step;
        now += frameMs;
        return trail.Advance(x, 0.f, now, settings);
    }
};
} // namespace

TEST_CASE("The echo's PC and phone settings")
{
    CHECK(StartAlpha == 0.6f);
    CHECK(Settings{}.startAlpha == StartAlpha);

    constexpr Settings pc = PcSettings();
    CHECK(pc.spacing == 70.f);
    CHECK(pc.lifetimeMs == 450.0);
    CHECK(pc.maxAlive == 3);
    CHECK(pc.startAlpha == StartAlpha);
    CHECK(pc.jump == 300.f);

    constexpr Settings phone = PhoneSettings();
    CHECK(phone.spacing == 110.f);
    CHECK(phone.lifetimeMs == 350.0);
    CHECK(phone.maxAlive == 2);
    CHECK(phone.startAlpha == StartAlpha);
    CHECK(phone.jump == 300.f);
}

TEST_CASE("The first position takes no copy")
{
    Trail trail;
    CHECK(trail.Advance(1000.f, 1000.f, 0.0, PcSettings()) == -1);
    CHECK(trail.AliveCount(0.0, PcSettings()) == 0);
}

TEST_CASE("A copy comes every 70 units of path on the PC")
{
    Walker walk;
    CHECK(walk.trail.Advance(0.f, 0.f, 0.0, walk.settings) == -1);
    // 10 units a frame: the copy is due on the seventh step (70 units).
    for (int i = 0; i < 6; ++i)
        CHECK(walk.Step(10.f) == -1);
    CHECK(walk.Step(10.f) == 0);
    CHECK(walk.trail.AliveCount(walk.now, walk.settings) == 1);
    // The next 70 units bring the next slot.
    for (int i = 0; i < 6; ++i)
        CHECK(walk.Step(10.f) == -1);
    CHECK(walk.Step(10.f) == 1);
    CHECK(walk.trail.AliveCount(walk.now, walk.settings) == 2);
}

TEST_CASE("The path counts in two dimensions and keeps what is left over")
{
    Trail trail;
    const Settings pc = PcSettings();
    trail.Advance(0.f, 0.f, 0.0, pc);
    // A 3-4-5 step of 50 units, then 50 more: 100 units, one copy, 30 left over.
    CHECK(trail.Advance(30.f, 40.f, 16.0, pc) == -1);
    CHECK(trail.Advance(60.f, 80.f, 32.0, pc) == 0);
    // 40 more units reach 70 again.
    CHECK(trail.Advance(60.f, 120.f, 48.0, pc) == 1);
}

TEST_CASE("Phones take a copy every 110 units")
{
    Walker walk;
    walk.settings = PhoneSettings();
    walk.trail.Advance(0.f, 0.f, 0.0, walk.settings);
    for (int i = 0; i < 10; ++i)
        CHECK(walk.Step(10.f) == -1);
    CHECK(walk.Step(10.f) == 0);
}

TEST_CASE("Standing still takes no copy")
{
    Walker walk;
    walk.trail.Advance(0.f, 0.f, 0.0, walk.settings);
    for (int i = 0; i < 200; ++i)
        CHECK(walk.Step(0.f) == -1);
    CHECK(walk.trail.AliveCount(walk.now, walk.settings) == 0);
}

TEST_CASE("A copy fades from 0.6 to nothing over its life")
{
    Trail trail;
    const Settings pc = PcSettings();
    trail.Advance(0.f, 0.f, 0.0, pc);
    REQUIRE(trail.Advance(70.f, 0.f, 1000.0, pc) == 0);

    CHECK(trail.AlphaOf(0, 1000.0, pc) == doctest::Approx(0.6f));
    CHECK(trail.AlphaOf(0, 1225.0, pc) == doctest::Approx(0.3f));
    CHECK(trail.IsAlive(0, 1449.0, pc));
    CHECK(trail.AlphaOf(0, 1449.0, pc) > 0.f);
    CHECK_FALSE(trail.IsAlive(0, 1450.0, pc));
    CHECK(trail.AlphaOf(0, 1450.0, pc) == 0.f);
    CHECK(trail.AlphaOf(0, 5000.0, pc) == 0.f);
    // A clock before the copy (never in the game) doesn't show it.
    CHECK_FALSE(trail.IsAlive(0, 999.0, pc));

    // On phones a copy lives 350 ms.
    Trail phoneTrail;
    const Settings phone = PhoneSettings();
    phoneTrail.Advance(0.f, 0.f, 0.0, phone);
    REQUIRE(phoneTrail.Advance(110.f, 0.f, 0.0, phone) == 0);
    CHECK(phoneTrail.IsAlive(0, 349.0, phone));
    CHECK_FALSE(phoneTrail.IsAlive(0, 350.0, phone));
}

TEST_CASE("At most three copies on the PC: the oldest makes room")
{
    Trail trail;
    const Settings pc = PcSettings();
    trail.Advance(0.f, 0.f, 0.0, pc);
    // A copy every frame (a fast mount): 70 units a frame, 10 ms apart, all alive.
    CHECK(trail.Advance(70.f, 0.f, 10.0, pc) == 0);
    CHECK(trail.Advance(140.f, 0.f, 20.0, pc) == 1);
    CHECK(trail.Advance(210.f, 0.f, 30.0, pc) == 2);
    CHECK(trail.AliveCount(30.0, pc) == 3);
    // The fourth replaces the oldest (slot 0), then slot 1.
    CHECK(trail.Advance(280.f, 0.f, 40.0, pc) == 0);
    CHECK(trail.Advance(350.f, 0.f, 50.0, pc) == 1);
    CHECK(trail.AliveCount(50.0, pc) == 3);
}

TEST_CASE("At most two copies on phones")
{
    Trail trail;
    const Settings phone = PhoneSettings();
    trail.Advance(0.f, 0.f, 0.0, phone);
    CHECK(trail.Advance(110.f, 0.f, 10.0, phone) == 0);
    CHECK(trail.Advance(220.f, 0.f, 20.0, phone) == 1);
    CHECK(trail.Advance(330.f, 0.f, 30.0, phone) == 0);
    CHECK(trail.AliveCount(30.0, phone) == 2);
}

TEST_CASE("A dead slot is taken before the oldest live one")
{
    Trail trail;
    const Settings pc = PcSettings();
    trail.Advance(0.f, 0.f, 0.0, pc);
    CHECK(trail.Advance(70.f, 0.f, 0.0, pc) == 0);
    CHECK(trail.Advance(140.f, 0.f, 300.0, pc) == 1);
    // At 500 ms slot 0 (born at 0) is gone and slot 1 (born at 300) is not: the copy goes to slot 0, the next to 2.
    CHECK(trail.Advance(210.f, 0.f, 500.0, pc) == 0);
    CHECK(trail.Advance(280.f, 0.f, 510.0, pc) == 2);
    CHECK(trail.AliveCount(510.0, pc) == 3);
}

TEST_CASE("A jump of more than 300 units clears the trail")
{
    Trail trail;
    const Settings pc = PcSettings();
    trail.Advance(0.f, 0.f, 0.0, pc);
    CHECK(trail.Advance(70.f, 0.f, 10.0, pc) == 0);
    CHECK(trail.Advance(130.f, 0.f, 20.0, pc) == -1);
    CHECK(trail.AliveCount(20.0, pc) == 1);

    // A warp: no copy, the old ones are gone, the path starts over from the new place.
    CHECK(trail.Advance(5000.f, 5000.f, 30.0, pc) == -1);
    CHECK(trail.AliveCount(30.0, pc) == 0);
    CHECK(trail.Advance(5060.f, 5000.f, 40.0, pc) == -1);
    CHECK(trail.Advance(5070.f, 5000.f, 50.0, pc) == 0);

    // Exactly 300 units is still a step (a fast mount at a low frame rate).
    Trail fast;
    fast.Advance(0.f, 0.f, 0.0, pc);
    CHECK(fast.Advance(300.f, 0.f, 100.0, pc) == 0);
    CHECK(fast.Advance(601.f, 0.f, 200.0, pc) == -1);
    CHECK(fast.AliveCount(200.0, pc) == 0);
}

TEST_CASE("Clearing forgets the copies and the last position")
{
    Trail trail;
    const Settings pc = PcSettings();
    trail.Advance(0.f, 0.f, 0.0, pc);
    CHECK(trail.Advance(70.f, 0.f, 10.0, pc) == 0);
    trail.Clear();
    CHECK(trail.AliveCount(10.0, pc) == 0);
    CHECK_FALSE(trail.IsAlive(0, 10.0, pc));
    // After a clear the next position is a first one again, however far.
    CHECK(trail.Advance(9000.f, 0.f, 20.0, pc) == -1);
    CHECK(trail.Advance(9070.f, 0.f, 30.0, pc) == 0);
}

TEST_CASE("Slots out of range are never alive")
{
    Trail trail;
    const Settings pc = PcSettings();
    CHECK_FALSE(trail.IsAlive(-1, 0.0, pc));
    CHECK_FALSE(trail.IsAlive(MaxCopies, 0.0, pc));
    CHECK(trail.AlphaOf(7, 0.0, pc) == 0.f);

    // A copy in the PC's third slot isn't shown once the trail runs with the phones' two.
    trail.Advance(0.f, 0.f, 0.0, pc);
    trail.Advance(70.f, 0.f, 10.0, pc);
    trail.Advance(140.f, 0.f, 20.0, pc);
    CHECK(trail.Advance(210.f, 0.f, 30.0, pc) == 2);
    CHECK_FALSE(trail.IsAlive(2, 30.0, PhoneSettings()));
}

TEST_CASE("A copy on top of its character shows only in part")
{
    CHECK(Clearance == 30.f);
    CHECK(ClearanceFactor(0.f) == 0.f);
    CHECK(ClearanceFactor(15.f) == doctest::Approx(0.5f));
    CHECK(ClearanceFactor(30.f) == 1.f);
    CHECK(ClearanceFactor(70.f) == 1.f);
    // Nonsense never shows more than nothing.
    CHECK(ClearanceFactor(-5.f) == 0.f);
    CHECK(ClearanceFactor(std::numeric_limits<float>::quiet_NaN()) == 0.f);
    // No clearance: always all.
    CHECK(ClearanceFactor(0.f, 0.f) == 1.f);
    CHECK(ClearanceFactor(10.f, -1.f) == 1.f);
    CHECK(ClearanceFactor(55.f, 110.f) == doctest::Approx(0.5f));
}
