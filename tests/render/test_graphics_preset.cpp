#include "stdafx.h"

#include "doctest.h"

#include "Data/GameConfig/GameConfigValidation.h"
#include "Render/Look/GraphicsPreset.h"

#include <string_view>
#include <utility>
#include <vector>

using Render::Look::ChooseAutoGraphicsPreset;
using Render::Look::GraphicsPresetSettings;
using Render::Look::GraphicsSettings;
using Render::Look::MatchGraphicsPreset;

TEST_CASE("the preset table has the agreed rows [render][graphics-preset]")
{
    const GraphicsSettings low = GraphicsPresetSettings(GraphicsPreset::Low);
    CHECK(low == GraphicsSettings{LookPreset::Classic, false, false, false, false, 0, false, 0, 1, 0});
    const GraphicsSettings medium = GraphicsPresetSettings(GraphicsPreset::Medium);
    CHECK(medium == GraphicsSettings{LookPreset::Enhanced, true, false, true, true, 0, true, 30, 4, 0});
    const GraphicsSettings high = GraphicsPresetSettings(GraphicsPreset::High);
    CHECK(high == GraphicsSettings{LookPreset::Enhanced, true, true, true, true, 4, false, 30, 8, 1});
    const GraphicsSettings ultra = GraphicsPresetSettings(GraphicsPreset::Ultra);
    CHECK(ultra == GraphicsSettings{LookPreset::Enhanced, true, true, true, true, 8, false, 30, 16, 2});
    CHECK(low.sunShadows == 0);
    CHECK(medium.sunShadows == 0);
    CHECK(high.sunShadows == 1);
    CHECK(ultra.sunShadows == 2);
}

TEST_CASE("values equal to a row are that preset, anything else is Custom [render][graphics-preset]")
{
    for (const GraphicsPreset preset :
         {GraphicsPreset::Low, GraphicsPreset::Medium, GraphicsPreset::High, GraphicsPreset::Ultra})
    {
        CHECK(MatchGraphicsPreset(GraphicsPresetSettings(preset)) == preset);
    }

    GraphicsSettings changed = GraphicsPresetSettings(GraphicsPreset::High);
    changed.anisotropy = 4;
    CHECK(MatchGraphicsPreset(changed) == GraphicsPreset::Custom);
    changed = GraphicsPresetSettings(GraphicsPreset::Medium);
    changed.fxaa = false;
    CHECK(MatchGraphicsPreset(changed) == GraphicsPreset::Custom);
    changed = GraphicsPresetSettings(GraphicsPreset::Ultra);
    changed.sharpen = 0;
    CHECK(MatchGraphicsPreset(changed) == GraphicsPreset::Custom);
    changed = GraphicsPresetSettings(GraphicsPreset::High);
    changed.sunShadows = 2;
    CHECK(MatchGraphicsPreset(changed) == GraphicsPreset::Custom);
    changed = GraphicsPresetSettings(GraphicsPreset::Low);
    changed.look = LookPreset::Enhanced;
    CHECK(MatchGraphicsPreset(changed) == GraphicsPreset::Custom);
}

TEST_CASE("Auto picks Low for software renderers [render][graphics-preset]")
{
    CHECK(ChooseAutoGraphicsPreset("Microsoft Basic Render Driver") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("llvmpipe (LLVM 15.0.7, 256 bits)") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("SwiftShader Device (Subzero)") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("Microsoft Basic Render Driver (WARP)") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("Microsoft WARP Adapter") == GraphicsPreset::Low);
}

TEST_CASE("Auto picks Medium for integrated graphics and unknown names [render][graphics-preset]")
{
    CHECK(ChooseAutoGraphicsPreset("Intel(R) Iris(R) Xe Graphics") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon(TM) Graphics") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon Graphics") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon(TM) Vega 8 Graphics") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon(TM) RX Vega 11 Graphics") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon 780M Graphics") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon(TM) 610M") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("") == GraphicsPreset::Medium);
    CHECK(ChooseAutoGraphicsPreset("Unknown") == GraphicsPreset::Medium);
}

TEST_CASE("Auto picks Low for weak integrated graphics [render][graphics-preset]")
{
    CHECK(ChooseAutoGraphicsPreset("Intel(R) HD Graphics 520") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("Intel(R) HD Graphics") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("Intel(R) UHD Graphics 630") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("Intel(R) UHD Graphics") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon(TM) R5 Graphics") == GraphicsPreset::Low);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon R7 Graphics") == GraphicsPreset::Low);
}

TEST_CASE("a GPU is classed by its name [render][graphics-preset]")
{
    using Render::Look::ClassifyGpu;
    using Render::Look::GpuClass;
    using Render::Look::IsWeakGpu;
    CHECK(ClassifyGpu("Microsoft Basic Render Driver") == GpuClass::Software);
    CHECK(ClassifyGpu("Intel(R) UHD Graphics 620") == GpuClass::WeakIntegrated);
    CHECK(ClassifyGpu("Intel(R) Iris(R) Plus Graphics 640") == GpuClass::Integrated);
    CHECK(ClassifyGpu("Intel(R) Iris(R) Xe Graphics") == GpuClass::Integrated);
    CHECK(ClassifyGpu("AMD Radeon(TM) Vega 8 Graphics") == GpuClass::Integrated);
    CHECK(ClassifyGpu("") == GpuClass::Integrated);
    CHECK(ClassifyGpu("Intel(R) Arc(TM) A380 Graphics") == GpuClass::Discrete);
    CHECK(ClassifyGpu("AMD Radeon R7 200 Series") == GpuClass::Discrete);
    CHECK(ClassifyGpu("NVIDIA GeForce GT 710") == GpuClass::Discrete);
    CHECK(IsWeakGpu("llvmpipe (LLVM 15.0.7, 256 bits)"));
    CHECK(IsWeakGpu("Intel(R) HD Graphics 4600"));
    CHECK_FALSE(IsWeakGpu("Intel(R) Iris(R) Xe Graphics"));
    CHECK_FALSE(IsWeakGpu("NVIDIA GeForce RTX 3060"));
    CHECK_FALSE(IsWeakGpu("Unknown"));
}

TEST_CASE("Auto picks High for discrete cards, Intel Arc and Radeon RX [render][graphics-preset]")
{
    CHECK(ChooseAutoGraphicsPreset("NVIDIA GeForce RTX 3060") == GraphicsPreset::High);
    CHECK(ChooseAutoGraphicsPreset("Intel(R) Arc(TM) A770 Graphics") == GraphicsPreset::High);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon RX 6700 XT") == GraphicsPreset::High);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon RX Vega 64") == GraphicsPreset::High);
    CHECK(ChooseAutoGraphicsPreset("AMD Radeon Pro W6800") == GraphicsPreset::High);
    CHECK(ChooseAutoGraphicsPreset("Apple M2") == GraphicsPreset::High);
}

TEST_CASE("the preset name in config.ini reads in any case and falls back to Auto [render][graphics-preset]")
{
    using GameConfigValidation::FormatGraphicsPreset;
    using GameConfigValidation::ParseGraphicsPreset;
    using GameConfigValidation::ValidateGraphicsPreset;
    for (const GraphicsPreset preset : {GraphicsPreset::Auto, GraphicsPreset::Low, GraphicsPreset::Medium,
                                        GraphicsPreset::High, GraphicsPreset::Ultra, GraphicsPreset::Custom})
    {
        CHECK(ParseGraphicsPreset(FormatGraphicsPreset(preset)) == preset);
    }
    CHECK(ValidateGraphicsPreset(L" ultra ") == GraphicsPreset::Ultra);
    CHECK(ValidateGraphicsPreset(L"") == GraphicsPreset::Auto);
    CHECK(ValidateGraphicsPreset(L"Extreme") == GraphicsPreset::Auto);
}

TEST_CASE("sharpening is kept between 0 and 100 percent [render][graphics-preset]")
{
    CHECK(GameConfigValidation::ValidateSharpen(-5) == 0);
    CHECK(GameConfigValidation::ValidateSharpen(30) == 30);
    CHECK(GameConfigValidation::ValidateSharpen(250) == 100);
}

TEST_CASE("the sun shadows level is kept between 0 and 2 [render][graphics-preset]")
{
    CHECK(GameConfigValidation::ValidateSunShadows(-1) == 0);
    CHECK(GameConfigValidation::ValidateSunShadows(1) == 1);
    CHECK(GameConfigValidation::ValidateSunShadows(7) == 2);
}

TEST_CASE("a new installation's window is the largest display mode that fits the usable area [config][window]")
{
    using GameConfigValidation::ChooseWindowSize;
    const std::vector<std::pair<int, int>> modes{{3840, 2160}, {2560, 1440}, {1920, 1080}, {1600, 900},
                                                 {1280, 1024}, {1024, 768},  {800, 600}};
    CHECK(ChooseWindowSize(modes, 1920, 1040) == std::pair<int, int>{1600, 900});
    CHECK(ChooseWindowSize(modes, 3840, 2100) == std::pair<int, int>{2560, 1440});
    CHECK(ChooseWindowSize(modes, 1366, 728) == std::pair<int, int>{800, 600});
}

TEST_CASE("without a fitting display mode the window takes 90 percent of the usable area [config][window]")
{
    using GameConfigValidation::ChooseWindowSize;
    CHECK(ChooseWindowSize({}, 1000, 700) == std::pair<int, int>{900, 630});
    CHECK(ChooseWindowSize({{1920, 1080}}, 1000, 700) == std::pair<int, int>{900, 630});
}
