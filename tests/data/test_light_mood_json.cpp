#include "stdafx.h"

#include "doctest.h"

#include "Data/GameData/MapData/LightMood.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using Data::Items::HasErrors;
using Data::Items::ItemDataIssue;
using Data::Items::ItemDataIssueSeverity;
using Data::Maps::LightMood;
using Data::Maps::LightMoodData;

namespace
{
constexpr int Lorencia = 0;
constexpr int LostTower = 4;
constexpr int Atlans = 7;
constexpr int Icarus = 10;

struct ReadResult
{
    LightMoodData data;
    std::vector<ItemDataIssue> issues;
};

ReadResult Read(const std::string& text)
{
    ReadResult result;
    Data::Maps::ReadLightMoodJson(text, "LightMood.json", result.data, result.issues);
    return result;
}

std::string Messages(const std::vector<ItemDataIssue>& issues)
{
    std::string text;
    for (const ItemDataIssue& issue : issues)
    {
        text += issue.ToString() + "\n";
    }
    return text;
}

bool HasIssueAbout(const std::vector<ItemDataIssue>& issues, const std::string& field, const std::string& message)
{
    return std::any_of(issues.begin(), issues.end(), [&](const ItemDataIssue& issue)
                       { return issue.field == field && issue.message.find(message) != std::string::npos; });
}

// A mood with the haze values of a map entry (Remaster distance haze).
LightMood WithHaze(LightMood mood, double haze, double red, double green, double blue, double groundFog,
                   double groundFogHeight, double hazeSun)
{
    mood.haze = haze;
    mood.hazeR = red;
    mood.hazeG = green;
    mood.hazeB = blue;
    mood.groundFog = groundFog;
    mood.groundFogHeight = groundFogHeight;
    mood.hazeSun = hazeSun;
    return mood;
}

const std::string ValidFile = R"({
  "formatVersion": 1,
  "default": { "base": 0.7, "figures": 0.8, "dynamicGain": 1.2, "background": 1, "sun": 0.5 },
  "maps": [
    { "world": 10, "name": "Icarus", "background": 0.7 },
    { "world": 1, "base": 0.5, "figures": 0.6, "dynamicGain": 0.5, "background": 0, "sun": 0.3 }
  ]
})";
} // namespace

TEST_CASE("a light mood file gives the default and each map's mood [data][lightmood]")
{
    const ReadResult result = Read(ValidFile);

    CHECK_MESSAGE(result.issues.empty(), Messages(result.issues));
    CHECK(result.data.defaultMood == LightMood{0.7, 0.8, 1.2, 1.0, 0.5});
    REQUIRE(result.data.maps.size() == 2);
    CHECK(result.data.ForWorld(1) == LightMood{0.5, 0.6, 0.5, 0.0, 0.3});
}

TEST_CASE("a map entry overrides only the values it names [data][lightmood]")
{
    const ReadResult result = Read(ValidFile);

    CHECK(result.data.ForWorld(Icarus) == LightMood{0.7, 0.8, 1.2, 0.7, 0.5});
}

TEST_CASE("a map without an entry uses the default mood [data][lightmood]")
{
    const ReadResult result = Read(ValidFile);

    CHECK(result.data.ForWorld(Lorencia) == result.data.defaultMood);
}

TEST_CASE("a value out of range makes the whole file unusable [data][lightmood]")
{
    const ReadResult result = Read(R"({
      "formatVersion": 1,
      "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 1 },
      "maps": [ { "world": 10, "base": 0.1 } ]
    })");

    CHECK(HasErrors(result.issues));
    // The file, the map and the value are named.
    CHECK_MESSAGE(Messages(result.issues).find("LightMood.json: maps[world 10].base: is 0.1, must be from 0.2 to 1") !=
                      std::string::npos,
                  Messages(result.issues));
    // Classic everywhere, not a partial mood.
    CHECK(result.data.defaultMood == LightMood{});
    CHECK(result.data.maps.empty());
    CHECK(result.data.ForWorld(Icarus) == LightMood{});
}

TEST_CASE("every light mood value has its range [data][lightmood]")
{
    const ReadResult result = Read(R"({
      "formatVersion": 1,
      "default": { "base": 1.1, "figures": 1, "dynamicGain": 2.5, "background": -0.1, "sun": 1.5 }
    })");

    CHECK(HasIssueAbout(result.issues, "default.base", "must be from 0.2 to 1"));
    CHECK(HasIssueAbout(result.issues, "default.dynamicGain", "must be from 0.5 to 2"));
    CHECK(HasIssueAbout(result.issues, "default.background", "must be from 0 to 1"));
    CHECK(HasIssueAbout(result.issues, "default.sun", "must be from 0 to 1"));
    CHECK(result.data.defaultMood == LightMood{});
}

TEST_CASE("figures are never darker than the base [data][lightmood]")
{
    const ReadResult result = Read(R"({
      "formatVersion": 1,
      "default": { "base": 0.7, "figures": 0.6, "dynamicGain": 1, "background": 1, "sun": 1 }
    })");

    CHECK(HasIssueAbout(result.issues, "default.figures", "must be from 0.7 to 1"));
}

TEST_CASE("the default needs all five values [data][lightmood]")
{
    const ReadResult result = Read(R"({ "formatVersion": 1, "default": { "base": 0.7 } })");

    CHECK(HasIssueAbout(result.issues, "default.figures", "missing"));
    CHECK(HasIssueAbout(result.issues, "default.dynamicGain", "missing"));
    CHECK(HasIssueAbout(result.issues, "default.background", "missing"));
    CHECK(HasIssueAbout(result.issues, "default.sun", "missing"));
}

TEST_CASE("a map entry needs a world, once [data][lightmood]")
{
    const ReadResult result = Read(R"({
      "formatVersion": 1,
      "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 1 },
      "maps": [ { "base": 0.8 }, { "world": 256 }, { "world": 3 }, { "world": 3 } ]
    })");

    CHECK(HasIssueAbout(result.issues, "maps[0].world", "whole number from 0 to 255"));
    CHECK(HasIssueAbout(result.issues, "maps[1].world", "whole number from 0 to 255"));
    CHECK(HasIssueAbout(result.issues, "maps[world 3]", "more than once"));
    CHECK(result.data.maps.empty());
}

TEST_CASE("an unknown field is a warning and keeps the file usable [data][lightmood]")
{
    const ReadResult result = Read(R"({
      "formatVersion": 1,
      "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 1, "fog": 1 }
    })");

    REQUIRE(result.issues.size() == 1);
    CHECK(result.issues[0].severity == ItemDataIssueSeverity::Warning);
    CHECK(result.issues[0].field == "default.fog");
    CHECK(result.data.defaultMood.base == doctest::Approx(0.7));
}

TEST_CASE("a broken or missing light mood file is an error [data][lightmood]")
{
    CHECK(HasErrors(Read("{ not json").issues));
    CHECK(HasErrors(Read(R"({ "formatVersion": 2, "default": {} })").issues));

    const Data::Maps::LightMoodLoadResult missing =
        Data::Maps::LoadLightMoodFile(std::filesystem::temp_directory_path() / "mu_no_such_light_mood.json");
    CHECK(HasErrors(missing.issues));
    CHECK_MESSAGE(Messages(missing.issues).find("could not be read") != std::string::npos, Messages(missing.issues));
    CHECK(missing.data.defaultMood == LightMood{});
}

TEST_CASE("the shipped light mood file has the tuned values [data][lightmood]")
{
    // MU_TEST_DATA_DIR is the client folder's Data; the file name starts with it.
    const std::filesystem::path file =
        std::filesystem::path(MU_TEST_DATA_DIR).parent_path() / Data::Maps::LightMoodFile;
    const Data::Maps::LightMoodLoadResult loaded = Data::Maps::LoadLightMoodFile(file);

    CHECK_MESSAGE(loaded.issues.empty(), Messages(loaded.issues));
    // The tuned Enhanced look: base, figures, dynamicGain, background, sun,
    // shadowKeep, shadeCool, brightScale.
    CHECK(loaded.data.defaultMood == LightMood{0.45, 0.7, 1.7, 1.0, 0.8, 0.8, 0.1, 0.8});
    // The haze values: haze, hazeR/G/B, groundFog, groundFogHeight, hazeSun.
    CHECK(loaded.data.ForWorld(Icarus) ==
          WithHaze(LightMood{0.7, 0.7, 1.7, 0.7, 0.8, 0.8, 0.1, 0.95}, 0.16, 0.5, 0.62, 0.86, 0.0, 220.0, 0.1));
    CHECK(loaded.data.ForWorld(LostTower) ==
          WithHaze(LightMood{0.45, 0.7, 1.7, 1.0, 1.0, 0.8, 0.1, 0.8}, 0.1, 0.34, 0.28, 0.4, 0.1, 150.0, 0.0));
    // Atlans draws its ground after the effects: no haze there.
    CHECK(loaded.data.ForWorld(Atlans) == LightMood{0.8, 0.8, 1.7, 1.0, 1.0, 0.8, 0.1, 1.0});
    CHECK(loaded.data.ForWorld(Lorencia) ==
          WithHaze(loaded.data.defaultMood, 0.18, 0.62, 0.7, 0.66, 0.06, 220.0, 0.12));
    // Maps without an entry (arenas, events) keep no haze.
    CHECK(loaded.data.defaultMood.haze == 0.0);
    CHECK(loaded.data.defaultMood.groundFog == 0.0);
}

TEST_CASE("the haze values are optional and range-checked [data][lightmood]")
{
    const ReadResult result = Read(R"({
  "formatVersion": 1,
  "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 0.6 },
  "maps": [ { "world": 3, "name": "Noria", "haze": 0.2, "hazeR": 0.6, "hazeG": 0.72, "hazeB": 0.62,
              "groundFog": 0.14, "groundFogHeight": 200, "hazeSun": 0.1 } ]
})");
    REQUIRE_FALSE(HasErrors(result.issues));
    CHECK(result.data.defaultMood.haze == 0.0);
    CHECK(result.data.defaultMood.hazeR == doctest::Approx(0.6));
    CHECK(result.data.defaultMood.groundFogHeight == doctest::Approx(220.0));
    const LightMood& noria = result.data.ForWorld(3);
    CHECK(noria.haze == doctest::Approx(0.2));
    CHECK(noria.hazeG == doctest::Approx(0.72));
    CHECK(noria.groundFog == doctest::Approx(0.14));
    CHECK(noria.groundFogHeight == doctest::Approx(200.0));
    CHECK(noria.hazeSun == doctest::Approx(0.1));

    const ReadResult outOfRange = Read(R"({
  "formatVersion": 1,
  "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 0.6, "haze": 0.6,
               "hazeR": 1.2, "hazeG": -0.1, "hazeB": 2, "groundFog": 0.7, "groundFogHeight": 20, "hazeSun": 0.8 }
})");
    CHECK(HasErrors(outOfRange.issues));
    CHECK(HasIssueAbout(outOfRange.issues, "default.haze", "must be from 0 to 0.5"));
    CHECK(HasIssueAbout(outOfRange.issues, "default.hazeR", "must be from 0 to 1"));
    CHECK(HasIssueAbout(outOfRange.issues, "default.hazeG", "must be from 0 to 1"));
    CHECK(HasIssueAbout(outOfRange.issues, "default.hazeB", "must be from 0 to 1"));
    CHECK(HasIssueAbout(outOfRange.issues, "default.groundFog", "must be from 0 to 0.5"));
    CHECK(HasIssueAbout(outOfRange.issues, "default.groundFogHeight", "must be from 50 to 600"));
    CHECK(HasIssueAbout(outOfRange.issues, "default.hazeSun", "must be from 0 to 0.5"));
    CHECK(outOfRange.data.defaultMood == LightMood{});
}

TEST_CASE("the shadow keep and the cool shade are optional and range-checked [data][lightmood]")
{
    const ReadResult result = Read(R"({
  "formatVersion": 1,
  "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 0.6 },
  "maps": [ { "world": 0, "name": "Lorencia", "shadowKeep": 0.9, "shadeCool": 0.1, "brightScale": 0.85 } ]
})");
    REQUIRE_FALSE(HasErrors(result.issues));
    CHECK(result.data.defaultMood.shadowKeep == 0.0);
    CHECK(result.data.ForWorld(Lorencia).shadowKeep == doctest::Approx(0.9));
    CHECK(result.data.ForWorld(Lorencia).shadeCool == doctest::Approx(0.1));
    CHECK(result.data.ForWorld(Lorencia).brightScale == doctest::Approx(0.85));
    CHECK(result.data.defaultMood.brightScale == 1.0);

    const ReadResult outOfRange = Read(R"({
  "formatVersion": 1,
  "default": { "base": 0.7, "figures": 0.7, "dynamicGain": 1, "background": 1, "sun": 0.6, "shadowKeep": 2, "shadeCool": -0.1, "brightScale": 1.2 }
})");
    CHECK(HasErrors(outOfRange.issues));
    CHECK(Messages(outOfRange.issues).find("default.shadowKeep") != std::string::npos);
    CHECK(Messages(outOfRange.issues).find("default.shadeCool") != std::string::npos);
    CHECK(Messages(outOfRange.issues).find("default.brightScale") != std::string::npos);
}
