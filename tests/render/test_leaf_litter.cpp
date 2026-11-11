#include "doctest.h"

#include "Render/Models/LeafLitter.h"

using Render::Models::LeafLitter::Floats;
using Render::Models::LeafLitter::IsLeafLitterTexture;
using Render::Models::LeafLitter::kMaxHeightAboveGround;

TEST_CASE("only the tree_08 picture is leaf litter [render][models][leaflitter]")
{
    CHECK(IsLeafLitterTexture("tree_08.tga"));
    CHECK(IsLeafLitterTexture("TREE_08.OZT"));
    CHECK(IsLeafLitterTexture("Data\\Object1\\tree_08.tga"));
    CHECK(IsLeafLitterTexture("Data/Object7/Tree_08.jpg"));

    CHECK_FALSE(IsLeafLitterTexture("tree_01.tga"));
    CHECK_FALSE(IsLeafLitterTexture("tree_080.tga"));
    CHECK_FALSE(IsLeafLitterTexture("tree_08"));
    CHECK_FALSE(IsLeafLitterTexture("tree_08."));
    CHECK_FALSE(IsLeafLitterTexture("my_tree_08.tga"));
    CHECK_FALSE(IsLeafLitterTexture("tree_08.tga\\wall.jpg"));
    CHECK_FALSE(IsLeafLitterTexture(""));
}

TEST_CASE("a sprig floats only well above the ground [render][models][leaflitter]")
{
    CHECK_FALSE(Floats(100.0f, 100.0f));
    CHECK_FALSE(Floats(100.0f + kMaxHeightAboveGround, 100.0f));
    CHECK(Floats(100.0f + kMaxHeightAboveGround + 1.0f, 100.0f));
    CHECK(Floats(583.0f, 171.0f));
    CHECK_FALSE(Floats(50.0f, 120.0f));
}
