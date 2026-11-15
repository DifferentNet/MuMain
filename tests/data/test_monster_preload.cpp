#include "stdafx.h"

#include "doctest.h"

#include "Engine/Monsters/MapMonsterList.h"
#include "Engine/Monsters/MonsterPreloadQueue.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using Engine::Monsters::MapMonsterList;
using Engine::Monsters::MonsterPreloadLimits;
using Engine::Monsters::MonsterPreloadQueue;
using Engine::Monsters::WarmUpResult;

namespace
{
constexpr std::uint64_t MiB = 1024ull * 1024ull;

MapMonsterList ParseText(const std::string& text)
{
    std::istringstream input(text);
    return MapMonsterList::Parse(input);
}

// A preload whose clock, texture memory and warm-up the test drives: every warm-up takes `msPerType`
// and adds `bytesPerType` of textures, and the types listed in `skipped` aren't warmed.
struct FakeGame
{
    double now = 0.0;
    std::uint64_t textureBytes = 0;
    double msPerType = 10.0;
    std::uint64_t bytesPerType = MiB;
    std::vector<int> skipped;
    std::vector<int> warmed;

    MonsterPreloadQueue MakeQueue(MonsterPreloadLimits limits)
    {
        return MonsterPreloadQueue(
            [this](int type) {
                now += msPerType;
                if (std::find(skipped.begin(), skipped.end(), type) != skipped.end())
                {
                    return WarmUpResult::Skipped;
                }
                textureBytes += bytesPerType;
                warmed.push_back(type);
                return WarmUpResult::Warmed;
            },
            [this] { return now; }, [this] { return textureBytes; }, limits);
    }
};

constexpr MonsterPreloadLimits RoomyLimits{1000.0, 40, 256 * MiB};
} // namespace

TEST_CASE("MapMonsterList: pairs by map in file order")
{
    const MapMonsterList list = ParseText("0 3\n0 1\n2 7\n0 2\n");
    CHECK(list.MapCount() == 2);
    CHECK(list.PairCount() == 4);
    CHECK(list.TypesOf(0) == std::vector<int>{3, 1, 2});
    CHECK(list.TypesOf(2) == std::vector<int>{7});
    CHECK(list.TypesOf(5).empty());
    CHECK(list.BadLineCount() == 0);
    CHECK(list.DuplicateCount() == 0);
}

TEST_CASE("MapMonsterList: comments, blank lines, CRLF and whitespace")
{
    const MapMonsterList list = ParseText("# header\r\n\r\n   # indented comment\n\t1\t4\r\n  1 5  \n");
    CHECK(list.TypesOf(1) == std::vector<int>{4, 5});
    CHECK(list.BadLineCount() == 0);
}

TEST_CASE("MapMonsterList: bad lines are counted and left out")
{
    const MapMonsterList list = ParseText("1\n1 2 3\nx 2\n1 y\n-1 2\n1 -2\n1 2 # trailing\n3 9\n");
    CHECK(list.BadLineCount() == 7);
    CHECK(list.PairCount() == 1);
    CHECK(list.TypesOf(3) == std::vector<int>{9});
    CHECK(list.TypesOf(1).empty());
}

TEST_CASE("MapMonsterList: a repeated pair is kept once")
{
    const MapMonsterList list = ParseText("4 1\n4 1\n4 2\n5 1\n4 1\n");
    CHECK(list.TypesOf(4) == std::vector<int>{1, 2});
    CHECK(list.TypesOf(5) == std::vector<int>{1});
    CHECK(list.DuplicateCount() == 2);
    CHECK(list.PairCount() == 3);
}

TEST_CASE("MonsterPreloadQueue: everything fits")
{
    FakeGame game;
    MonsterPreloadQueue queue = game.MakeQueue(RoomyLimits);
    queue.Begin({1, 2, 3});
    const auto& stats = queue.RunFor(1000.0);
    CHECK(game.warmed == std::vector<int>{1, 2, 3});
    CHECK(stats.warmed == 3);
    CHECK(stats.skipped == 0);
    CHECK(stats.left == 0);
    CHECK(stats.elapsedMs == doctest::Approx(30.0));
    CHECK(stats.textureBytes == 3 * MiB);
    CHECK_FALSE(stats.textureLimitReached);
    CHECK_FALSE(queue.HasPending());
}

TEST_CASE("MonsterPreloadQueue: types past the cap are never queued")
{
    FakeGame game;
    MonsterPreloadQueue queue = game.MakeQueue({1000.0, 2, 256 * MiB});
    queue.Begin({1, 2, 3, 4, 5});
    CHECK(queue.Stats().overLimit == 3);
    CHECK(queue.Stats().left == 2);
    queue.RunFor(1000.0);
    CHECK(game.warmed == std::vector<int>{1, 2});
    CHECK_FALSE(queue.HasPending());
}

TEST_CASE("MonsterPreloadQueue: the time budget leaves the rest for the frames")
{
    FakeGame game;
    game.msPerType = 40.0;
    MonsterPreloadQueue queue = game.MakeQueue(RoomyLimits);
    queue.Begin({1, 2, 3, 4, 5});

    // Checked before each type: at 0, 40 and 80 ms there is time left, at 120 there isn't.
    const auto& stats = queue.RunFor(100.0);
    CHECK(stats.warmed == 3);
    CHECK(stats.left == 2);
    CHECK(queue.HasPending());

    queue.RunOne();
    CHECK(queue.Stats().warmed == 4);
    CHECK(queue.Stats().left == 1);
    queue.RunOne();
    CHECK(queue.Stats().warmed == 5);
    CHECK_FALSE(queue.HasPending());
    CHECK(queue.Stats().elapsedMs == doctest::Approx(200.0));

    // Nothing left: one more step does nothing.
    queue.RunOne();
    CHECK(game.warmed.size() == 5);
}

TEST_CASE("MonsterPreloadQueue: the texture limit stops the preload")
{
    FakeGame game;
    game.bytesPerType = 100 * MiB;
    MonsterPreloadQueue queue = game.MakeQueue(RoomyLimits);
    queue.Begin({1, 2, 3, 4, 5});
    const auto& stats = queue.RunFor(1000.0);
    CHECK(game.warmed == std::vector<int>{1, 2, 3});
    CHECK(stats.textureLimitReached);
    CHECK(stats.left == 0);
    CHECK_FALSE(queue.HasPending());
}

TEST_CASE("MonsterPreloadQueue: textures counted from Begin, never below nothing")
{
    FakeGame game;
    game.textureBytes = 900 * MiB;
    MonsterPreloadQueue queue = game.MakeQueue(RoomyLimits);
    queue.Begin({1});
    queue.RunFor(1000.0);
    CHECK(queue.Stats().textureBytes == MiB);

    game.bytesPerType = 0;
    queue.Begin({2});
    game.textureBytes -= 50 * MiB;
    queue.RunOne();
    CHECK(queue.Stats().textureBytes == 0);
}

TEST_CASE("MonsterPreloadQueue: skipped types are counted apart")
{
    FakeGame game;
    game.skipped = {2, 4};
    MonsterPreloadQueue queue = game.MakeQueue(RoomyLimits);
    queue.Begin({1, 2, 3, 4});
    const auto& stats = queue.RunFor(1000.0);
    CHECK(stats.warmed == 2);
    CHECK(stats.skipped == 2);
    CHECK(game.warmed == std::vector<int>{1, 3});
}

TEST_CASE("MonsterPreloadQueue: Clear forgets what is left and Begin starts over")
{
    FakeGame game;
    game.msPerType = 100.0;
    MonsterPreloadQueue queue = game.MakeQueue(RoomyLimits);
    queue.Begin({1, 2, 3});
    queue.RunFor(50.0);
    CHECK(queue.Stats().left == 2);
    queue.Clear();
    CHECK_FALSE(queue.HasPending());
    CHECK(queue.Stats().left == 0);
    queue.RunOne();
    CHECK(game.warmed == std::vector<int>{1});

    queue.Begin({7});
    CHECK(queue.Stats().warmed == 0);
    CHECK(queue.Stats().left == 1);
    queue.RunOne();
    CHECK(game.warmed == std::vector<int>{1, 7});
}

#ifdef MU_TEST_DATA_DIR
TEST_CASE("Shipped MapMonsters.txt: every line valid, every type a client monster")
{
    const std::filesystem::path path = std::filesystem::path(MU_TEST_DATA_DIR) / "Local" / "MapMonsters.txt";
    std::ifstream file(path);
    REQUIRE_MESSAGE(file.good(), path.string());
    const MapMonsterList list = MapMonsterList::Parse(file);
    CHECK(list.BadLineCount() == 0);
    CHECK(list.DuplicateCount() == 0);
    CHECK(list.PairCount() > 0);

    constexpr int MostMaps = 256;
    for (int map = 0; map < MostMaps; ++map)
    {
        const std::vector<int>& types = list.TypesOf(map);
        CHECK(types.size() <= 40);
        for (const int type : types)
        {
            CHECK(type >= MONSTER_START);
            CHECK(type <= MONSTER_END);
        }
    }
}
#endif
