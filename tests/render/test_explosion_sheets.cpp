// The explosion sheets in src/bin/Data hold as many frames as FlipbookBlend.h says: a cell past the
// count is solid white, so a blend into it would draw a white square.
#include <doctest.h>
#include <turbojpeg.h>

#include "Render/Effects/FlipbookBlend.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
namespace Flipbook = Render::Effects::FlipbookBlend;

// Bytes in front of the JPEG in an OZJ file (CGlobalBitmap::OpenJpegTurbo skips them).
constexpr std::size_t kOzjHeaderBytes = 24;
// The sheets are 4x4 grids of frames.
constexpr int kSheetColumns = 4;
constexpr int kSheetCells = kSheetColumns * kSheetColumns;
// A cell this bright on average is the solid white filler (255); the brightest fire frame is near 75.
constexpr double kWhiteCellMean = 250.0;
constexpr double kFireCellMeanAtMost = 128.0;

struct Sheet
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgb;
};

Sheet DecodeOzj(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    REQUIRE(bytes.size() > kOzjHeaderBytes);
    const unsigned char* jpeg = bytes.data() + kOzjHeaderBytes;
    const auto jpegSize = static_cast<unsigned long>(bytes.size() - kOzjHeaderBytes);

    tjhandle handle = tjInitDecompress();
    REQUIRE(handle != nullptr);
    Sheet sheet;
    int subsampling = 0;
    int colorspace = 0;
    REQUIRE(tjDecompressHeader3(handle, jpeg, jpegSize, &sheet.width, &sheet.height, &subsampling, &colorspace) == 0);
    sheet.rgb.resize(static_cast<std::size_t>(sheet.width) * sheet.height * 3);
    REQUIRE(tjDecompress2(handle, jpeg, jpegSize, sheet.rgb.data(), sheet.width, 0, sheet.height, TJPF_RGB, 0) == 0);
    tjDestroy(handle);
    return sheet;
}

// The mean of all channels over a cell.
double CellMean(const Sheet& sheet, const int cell)
{
    const int cellWidth = sheet.width / kSheetColumns;
    const int cellHeight = sheet.height / kSheetColumns;
    const int left = cell % kSheetColumns * cellWidth;
    const int top = cell / kSheetColumns * cellHeight;
    double sum = 0.0;
    int count = 0;
    for (int y = top; y < top + cellHeight; ++y)
    {
        for (int x = left; x < left + cellWidth; ++x)
        {
            const std::size_t at = (static_cast<std::size_t>(y) * sheet.width + x) * 3;
            sum += sheet.rgb[at] + sheet.rgb[at + 1] + sheet.rgb[at + 2];
            count += 3;
        }
    }
    return sum / count;
}

void CheckSheetFrames(const std::string& name, const int frames)
{
    CAPTURE(name);
    const Sheet sheet = DecodeOzj(std::filesystem::path(MU_TEST_DATA_DIR) / "Effect" / name);
    REQUIRE(sheet.width >= kSheetColumns);
    REQUIRE(sheet.height >= kSheetColumns);
    for (int cell = 0; cell < kSheetCells; ++cell)
    {
        CAPTURE(cell);
        if (cell < frames)
        {
            CHECK(CellMean(sheet, cell) <= kFireCellMeanAtMost);
        }
        else
        {
            CHECK(CellMean(sheet, cell) >= kWhiteCellMean);
        }
    }
}
} // namespace

TEST_CASE("the explosion sheets have fire in their frames and white past them [render][flipbook_blend]")
{
    CheckSheetFrames("Explotion01.OZJ", Flipbook::kExplosionSheetFrames);
    CheckSheetFrames("explotion01mono.OZJ", Flipbook::kExplosionSheetFrames);
}

TEST_CASE("the Sahamutt explosion sheet has fire in all its cells [render][flipbook_blend]")
{
    CheckSheetFrames("loungexflow.OZJ", Flipbook::kSahamuttExplosionSheetFrames);
}
