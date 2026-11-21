#include "doctest.h"

#include "UI/Inventory/ItemCellRarityClass.h"

#include <string>

using GameLogic::Items::Rarity::Tier;
using GameLogic::Items::Rarity::UpgradeBand;
using UI::Inventory::EdgeClasses;
using UI::Inventory::RarityCellClasses;

namespace
{
std::string Classes(Tier tier, UpgradeBand band)
{
    return RarityCellClasses(tier, band);
}

// The edges a cell of an item `width` x `height` cells big lies on, at (x, y) inside it.
std::string Edges(int x, int y, int width, int height)
{
    return EdgeClasses(y == 0, y == height - 1, x == 0, x == width - 1);
}
} // namespace

TEST_CASE("only excellent, ancient and socket items tint their cells [item_cell_classes]")
{
    CHECK(Classes(Tier::Excellent, UpgradeBand::None) == " rar-exc rf");
    CHECK(Classes(Tier::Ancient, UpgradeBand::None) == " rar-anc rf");
    CHECK(Classes(Tier::Socket, UpgradeBand::None) == " rar-socket rf");
    for (Tier tier : {Tier::Normal, Tier::Magic, Tier::Fine, Tier::Special, Tier::Divine})
    {
        CAPTURE(static_cast<int>(tier));
        CHECK(Classes(tier, UpgradeBand::None).empty());
    }
}

TEST_CASE("an upgraded item gets a frame whatever its rarity [item_cell_classes]")
{
    for (Tier tier : {Tier::Normal, Tier::Magic, Tier::Fine, Tier::Special, Tier::Divine})
    {
        CAPTURE(static_cast<int>(tier));
        CHECK(Classes(tier, UpgradeBand::Plus7) == " rf up-fine");
        CHECK(Classes(tier, UpgradeBand::Gold) == " rf up-gold");
    }
    CHECK(Classes(Tier::Excellent, UpgradeBand::Plus7) == " rar-exc rf up-fine");
    CHECK(Classes(Tier::Excellent, UpgradeBand::Gold) == " rar-exc rf up-gold");
    CHECK(Classes(Tier::Ancient, UpgradeBand::Plus7) == " rar-anc rf up-fine");
    CHECK(Classes(Tier::Ancient, UpgradeBand::Gold) == " rar-anc rf up-gold");
    CHECK(Classes(Tier::Socket, UpgradeBand::Plus7) == " rar-socket rf up-fine");
    CHECK(Classes(Tier::Socket, UpgradeBand::Gold) == " rar-socket rf up-gold");
}

TEST_CASE("no class could be taken for a tint or the pad's item marker [item_cell_classes]")
{
    // UI/HUD/PadTargets reads "tint-" (an occupied cell) and "pad-item-<n>" out of a cell's classes.
    for (Tier tier : {Tier::Normal, Tier::Excellent, Tier::Ancient, Tier::Socket})
        for (UpgradeBand band : {UpgradeBand::None, UpgradeBand::Plus7, UpgradeBand::Gold})
        {
            const std::string classes = Classes(tier, band);
            CHECK(classes.find("tint-") == std::string::npos);
            CHECK(classes.find("pad-item-") == std::string::npos);
            CHECK((classes.empty() || classes.front() == ' '));
        }
}

TEST_CASE("a one-cell item's cell lies on all four edges [item_cell_classes]")
{
    CHECK(Edges(0, 0, 1, 1) == " re-t re-b re-l re-r");
    CHECK(std::string(EdgeClasses(false, false, false, false)).empty());
}

TEST_CASE("a 2 x 3 item frames only its outer edges [item_cell_classes]")
{
    // Columns x = 0..1, rows y = 0..2.
    CHECK(Edges(0, 0, 2, 3) == " re-t re-l");
    CHECK(Edges(1, 0, 2, 3) == " re-t re-r");
    CHECK(Edges(0, 1, 2, 3) == " re-l");
    CHECK(Edges(1, 1, 2, 3) == " re-r");
    CHECK(Edges(0, 2, 2, 3) == " re-b re-l");
    CHECK(Edges(1, 2, 2, 3) == " re-b re-r");
}

TEST_CASE("an inner cell has no edge, a one-column item both sides [item_cell_classes]")
{
    CHECK(Edges(1, 1, 3, 3).empty());
    CHECK(Edges(0, 0, 1, 4) == " re-t re-l re-r");
    CHECK(Edges(0, 2, 1, 4) == " re-l re-r");
    CHECK(Edges(0, 3, 1, 4) == " re-b re-l re-r");
    CHECK(Edges(2, 0, 4, 1) == " re-t re-b");
}
