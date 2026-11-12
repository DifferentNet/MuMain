#include "stdafx.h"

#include "Render/PostProcessing/MapColorGrading.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Render/PostProcessing/ColorGradingTable.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Textures/HdTextures.h"

#include <optional>

namespace Render::PostProcessing
{
namespace
{
std::optional<int> s_selectedMap;

void ApplyTable(const GradingTableLoad& table, int mapNumber, const std::filesystem::path& path)
{
    const auto logger = mu::log::Get("render");
    switch (table.status)
    {
    case GradingTableStatus::Missing:
        logger->info("Colour grading: map {} has no table ({}); not graded", mapNumber, path.string());
        break;
    case GradingTableStatus::Invalid:
        logger->warn("Colour grading: {} is invalid ({}); map {} not graded", path.string(), table.error, mapNumber);
        break;
    case GradingTableStatus::Loaded:
        if (mu::GetRenderer().SetColorGradingTable(table.rgba))
        {
            logger->info("Colour grading: map {} graded by {}", mapNumber, path.string());
            return;
        }
        logger->warn("Colour grading: {} could not be uploaded; map {} not graded", path.string(), mapNumber);
        break;
    }
    (void)mu::GetRenderer().SetColorGradingTable({});
}
} // namespace

void SelectMapColorGrading(int mapNumber)
{
    if (s_selectedMap == mapNumber)
    {
        return;
    }
    s_selectedMap = mapNumber;

    // The Remaster client (HD textures on) has its own tone: the map's Remaster table, else the
    // Remaster default; a broken or missing Remaster table falls back to the Classic one.
    if (Render::Textures::Hd::Enabled())
    {
        for (const std::filesystem::path& path : {GetRemasterGradingTablePath(mapNumber), GetRemasterDefaultGradingTablePath()})
        {
            const GradingTableLoad table = LoadGradingTable(path);
            if (table.status == GradingTableStatus::Loaded)
            {
                ApplyTable(table, mapNumber, path);
                return;
            }
            if (table.status == GradingTableStatus::Invalid)
            {
                mu::log::Get("render")->warn("Colour grading: {} is invalid ({}); trying the next table", path.string(), table.error);
            }
        }
        mu::log::Get("render")->info("Colour grading: no Remaster table for map {}; using the Classic one", mapNumber);
    }

    const std::filesystem::path path = GetGradingTablePath(mapNumber);
    ApplyTable(LoadGradingTable(path), mapNumber, path);
}

} // namespace Render::PostProcessing
