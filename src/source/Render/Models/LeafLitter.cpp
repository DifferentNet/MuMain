#include "Render/Models/LeafLitter.h"

#include <cctype>
#include <cstddef>

namespace Render::Models::LeafLitter
{
bool IsLeafLitterTexture(std::string_view textureName)
{
    const std::size_t slash = textureName.find_last_of("\\/");
    const std::string_view file = slash == std::string_view::npos ? textureName : textureName.substr(slash + 1);
    constexpr std::string_view kPrefix = "tree_08.";
    if (file.size() <= kPrefix.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < kPrefix.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(file[i])) != kPrefix[i])
        {
            return false;
        }
    }
    return true;
}

bool Floats(float objectZ, float groundZ)
{
    return objectZ - groundZ > kMaxHeightAboveGround;
}
} // namespace Render::Models::LeafLitter
