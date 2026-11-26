#include "stdafx.h"

#include "doctest.h"

#include "Core/Globals/_enum.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "World/MapInfra/MapManager.h"

#include "TestFiles.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// The map object files: a broken record makes no object. The original Arena (World7\EncTerrain7.obj)
// has one with type -515; drawing it wrote outside Models and crashed the client (2026-11-26).
namespace
{
const std::filesystem::path ArenaObjects = std::filesystem::path(MU_TEST_DATA_DIR) / "World7" / "EncTerrain7.obj";

// A record of the file: short type, position, angle, float scale.
constexpr size_t RecordBytes = sizeof(short) + 2 * sizeof(vec3_t) + sizeof(float);

std::vector<short> RecordTypes(const std::filesystem::path& path)
{
    std::string file = TestFiles::ReadWholeFile(path);
    std::vector<BYTE> enc(file.begin(), file.end());
    const int size = MapFileDecrypt(nullptr, enc.data(), static_cast<int>(enc.size()));
    std::vector<BYTE> data(size);
    MapFileDecrypt(data.data(), enc.data(), static_cast<int>(enc.size()));

    short count = 0;
    std::memcpy(&count, data.data() + 2, sizeof(count));
    std::vector<short> types;
    for (int i = 0; i < count; ++i)
    {
        short type = 0;
        std::memcpy(&type, data.data() + 4 + i * RecordBytes, sizeof(type));
        types.push_back(type);
    }
    return types;
}

std::vector<OBJECT*> AllObjects()
{
    std::vector<OBJECT*> objects;
    for (OBJECT_BLOCK& block : ObjectBlock)
    {
        for (OBJECT* o = block.Head; o != nullptr; o = o->Next)
        {
            objects.push_back(o);
        }
    }
    return objects;
}

void DeleteAllObjects()
{
    for (OBJECT_BLOCK& block : ObjectBlock)
    {
        while (block.Head != nullptr)
        {
            DeleteObject(block.Head, &block);
        }
    }
}

// The objects the test made are deleted, and the world it set is put back, at the end of each test.
struct ObjectScope
{
    int previousWorld = gMapManager.WorldActive;

    ObjectScope()
    {
        // The loader's per-map tweaks of a few model types are not under test; Lorencia's need no map state.
        gMapManager.WorldActive = WD_0LORENCIA;
    }

    ~ObjectScope()
    {
        DeleteAllObjects();
        // A few Lorencia types also make an operate that points at its object.
        for (OPERATE& operate : Operates)
        {
            operate.Live = false;
        }
        gMapManager.WorldActive = previousWorld;
    }
};
} // namespace

TEST_CASE("CreateObject makes no object of a type outside the model slots")
{
    ObjectScope scope;
    vec3_t position = {100.f, 100.f, 0.f};
    vec3_t angle = {0.f, 0.f, 0.f};

    CHECK(CreateObject(-515, position, angle) == nullptr);
    CHECK(CreateObject(-1, position, angle) == nullptr);
    CHECK(CreateObject(MAX_MODELS, position, angle) == nullptr);
    CHECK(AllObjects().empty());

    OBJECT* last = CreateObject(MAX_MODELS - 1, position, angle);
    REQUIRE(last != nullptr);
    CHECK(last->Type == MAX_MODELS - 1);
    OBJECT* first = CreateObject(0, position, angle);
    REQUIRE(first != nullptr);
    CHECK(first->Type == 0);
    CHECK(AllObjects().size() == 2);
}

TEST_CASE("The Arena's object file loads without its broken record")
{
    REQUIRE(std::filesystem::exists(ArenaObjects));
    const std::vector<short> types = RecordTypes(ArenaObjects);
    int broken = 0;
    for (short type : types)
    {
        if (type < 0 || type >= MAX_MODELS) ++broken;
    }
    // The shipped file still has the record that crashed the client; if the data gets fixed, this test
    // still checks the load, and this line can go.
    CHECK(broken == 1);

    ObjectScope scope;
    std::wstring path = ArenaObjects.wstring();
    CHECK(OpenObjectsEnc(path.data()) >= 0);

    const std::vector<OBJECT*> objects = AllObjects();
    for (const OBJECT* o : objects)
    {
        CHECK(o->Type >= 0);
        CHECK(o->Type < MAX_MODELS);
    }
    // Every good record is placed (all of the Arena's are inside the map), the broken one is not.
    CHECK(objects.size() == types.size() - broken);
}
