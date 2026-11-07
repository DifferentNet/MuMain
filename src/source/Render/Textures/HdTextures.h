#pragma once

#include <optional>
#include <string>
#include <string_view>

// HD textures: upscaled copies of the game's OZJ and OZT files (UpscaleMarker.h) in their own
// folder, Data\HD, at the same paths as the originals ("Data\World1\TileGrass01.OZJ" ->
// "Data\HD\World1\TileGrass01.OZJ"). The player switches them on in Options ([Render] HdTextures);
// a texture without an HD copy loads the original. The copies replace only the image the GPU
// draws: anything that reads a texture's pixels (canvases) keeps the original file.
namespace Render::Textures::Hd
{

// The HD copy's path for a file under Data, with the separator the path uses; nothing for a path
// outside Data or already under Data\HD. Either separator and any case.
[[nodiscard]] std::optional<std::wstring> HdCopyPath(std::wstring_view path);

// Read once from the config at start-up: a texture already loaded keeps its image.
void SetEnabled(bool enabled);
[[nodiscard]] bool Enabled();

// The file to load for `path`: its HD copy when HD textures are on and the copy is there,
// otherwise `path`.
[[nodiscard]] std::wstring Resolve(const std::wstring& path);

} // namespace Render::Textures::Hd
