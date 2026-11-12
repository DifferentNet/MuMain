// WaitingSoundLoads: the web build's sounds waiting for their files (MiniAudioBackend.cpp
// WaitForSoundFile, LoadWaitingSounds).

#include "doctest.h"

#include "Core/Platform/Audio/WaitingSoundLoads.h"

namespace
{
constexpr int Wind = 3;
constexpr int Rain = 4;
constexpr int Anvil = 9;

mu::WaitingSoundLoad Load(int slot, const char* path)
{
    return {slot, L"Data\\Sound\\x.wav", path, 1, false};
}
} // namespace

TEST_CASE("WaitingSoundLoads: first asked, first loaded")
{
    mu::WaitingSoundLoads loads;
    CHECK(loads.Empty());
    CHECK(loads.First() == nullptr);
    CHECK_FALSE(loads.TakeFirst().has_value());

    loads.Add(Load(Wind, "Data/Sound/aWind.wav"));
    loads.Add(Load(Rain, "Data/Sound/aRain.wav"));
    REQUIRE(loads.First() != nullptr);
    CHECK(loads.First()->slot == Wind);
    CHECK(loads.TakeFirst()->path == "Data/Sound/aWind.wav");
    CHECK(loads.TakeFirst()->path == "Data/Sound/aRain.wav");
    CHECK(loads.Empty());
}

TEST_CASE("WaitingSoundLoads: a slot loaded again waits with its last file")
{
    mu::WaitingSoundLoads loads;
    loads.Add(Load(Wind, "Data/Sound/aWind.wav"));
    loads.Add(Load(Rain, "Data/Sound/aRain.wav"));
    loads.Add(Load(Wind, "Data/Sound/aDungeon.wav"));
    CHECK(loads.Size() == 2);
    CHECK(loads.TakeFirst()->slot == Rain);
    const auto wind = loads.TakeFirst();
    REQUIRE(wind.has_value());
    CHECK(wind->slot == Wind);
    CHECK(wind->path == "Data/Sound/aDungeon.wav");
}

TEST_CASE("WaitingSoundLoads: loads whose files are here go first, the others keep waiting in order")
{
    mu::WaitingSoundLoads loads;
    loads.Add(Load(Wind, "Data/Music/x.wav"));
    loads.Add(Load(Rain, "Data/Sound/aRain.wav"));
    loads.Add(Load(Anvil, "Data/Music/y.wav"));
    const auto here = [](const mu::WaitingSoundLoad& load) { return load.path.starts_with("Data/Sound/"); };
    const auto rain = loads.TakeFirstReady(here);
    REQUIRE(rain.has_value());
    CHECK(rain->slot == Rain);
    CHECK_FALSE(loads.TakeFirstReady(here).has_value());
    CHECK(loads.Size() == 2);
    CHECK(loads.TakeFirst()->slot == Wind);
    CHECK(loads.TakeFirst()->slot == Anvil);
}

TEST_CASE("WaitingSoundLoads: a released slot waits no more")
{
    mu::WaitingSoundLoads loads;
    loads.Add(Load(Wind, "Data/Sound/aWind.wav"));
    loads.Add(Load(Anvil, "Data/Sound/nBlackSmith.wav"));
    CHECK(loads.Remove(Wind));
    CHECK_FALSE(loads.Remove(Rain)); // never waited: nothing happens
    CHECK(loads.Size() == 1);
    CHECK(loads.First()->slot == Anvil);
    loads.Clear();
    CHECK(loads.Empty());
}
