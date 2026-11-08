#include "doctest.h"

#include "Render/Textures/HdTextures.h"

using Render::Textures::Hd::HdCopyPath;

TEST_CASE("an HD copy lies under Data\\HD at the original's path [render][textures][hd]")
{
    CHECK(HdCopyPath(L"Data\\World1\\TileGrass01.OZJ") == std::wstring(L"Data\\HD\\World1\\TileGrass01.OZJ"));
    CHECK(HdCopyPath(L"data/Player/item1001_a.OZT") == std::wstring(L"data/HD/Player/item1001_a.OZT"));
    CHECK(HdCopyPath(L"DATA\\Item\\Sword01.OZJ") == std::wstring(L"DATA\\HD\\Item\\Sword01.OZJ"));
}

TEST_CASE("a path outside Data or already in Data\\HD has no HD copy [render][textures][hd]")
{
    CHECK_FALSE(HdCopyPath(L"Data\\HD\\World1\\TileGrass01.OZJ").has_value());
    CHECK_FALSE(HdCopyPath(L"Data/hd/World1/TileGrass01.OZJ").has_value());
    CHECK_FALSE(HdCopyPath(L"Interface\\Cursor.OZT").has_value());
    CHECK_FALSE(HdCopyPath(L"DataX\\World1\\TileGrass01.OZJ").has_value());
    CHECK_FALSE(HdCopyPath(L"Data\\").has_value());
    CHECK_FALSE(HdCopyPath(L"Data").has_value());
    CHECK_FALSE(HdCopyPath(L"").has_value());
    // A folder whose name only starts with HD is an ordinary folder.
    CHECK(HdCopyPath(L"Data\\HDX\\a.OZJ") == std::wstring(L"Data\\HD\\HDX\\a.OZJ"));
}
