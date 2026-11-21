// Developer tool: forges one raw RGBA sprite with a reforge preset, to look at the presets on real
// art without starting the game. Convert a decoded sprite to raw RGBA (width x height x 4 bytes, rows
// top-down) first, and the result back to an image to look at it. Prints the forge's own time on stderr.
//
//   sprite_reforge_preview <in.rgba> <width> <height> <scale> <preset> <out.rgba>

#include "Render/Textures/SpriteReforge.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
constexpr int ArgumentCount = 7;
constexpr int Components = 4;

int Fail(const char* message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != ArgumentCount)
        return Fail("usage: sprite_reforge_preview <in.rgba> <width> <height> <scale> <preset> <out.rgba>");
    const int width = std::stoi(argv[2]);
    const int height = std::stoi(argv[3]);
    const int scale = std::stoi(argv[4]);
    const auto preset = Render::Textures::Reforge::ParsePreset(argv[5]);
    if (!preset)
        return Fail("unknown preset");
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<std::uint8_t> pixels((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (width <= 0 || height <= 0 ||
        pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * Components)
        return Fail("the input is not width x height RGBA pixels");
    const auto start = std::chrono::steady_clock::now();
    Render::Textures::Reforge::Apply(*preset, pixels.data(), width, height, Components, scale);
    const std::chrono::duration<double, std::milli> forged = std::chrono::steady_clock::now() - start;
    std::fprintf(stderr, "forged in %.1f ms\n", forged.count());
    std::ofstream output(argv[6], std::ios::binary);
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    return output ? 0 : Fail("could not write the output");
}
