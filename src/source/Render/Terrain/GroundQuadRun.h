#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "Render/Renderer/MuRenderer.h"

namespace Render::Terrain
{

// The ground quads of one draw, gathered in a buffer that never reallocates. The ground appends thousands
// of faces a frame, and a std::vector range insert per face showed up in the /play profile.
class GroundQuadRun
{
public:
    static constexpr std::size_t kQuadVertices = 4;
    // Quads per ground draw: well under the SDL_gpu renderer's k_MaxQuads (4096), which clamps a longer draw.
    static constexpr std::size_t kMaxQuads = 1024;
    static constexpr std::size_t kMaxVertices = kMaxQuads * kQuadVertices;

    // The caller flushes a full run first: appending to a full run is ignored.
    void Append(const mu::Vertex3D (&quad)[kQuadVertices])
    {
        if (Full())
            return;
        std::copy_n(quad, kQuadVertices, m_vertices.data() + m_count);
        m_count += kQuadVertices;
    }

    [[nodiscard]] bool Full() const { return m_count >= kMaxVertices; }
    [[nodiscard]] bool Empty() const { return m_count == 0; }
    [[nodiscard]] std::span<const mu::Vertex3D> Vertices() const { return {m_vertices.data(), m_count}; }
    void Clear() { m_count = 0; }

private:
    std::array<mu::Vertex3D, kMaxVertices> m_vertices{};
    std::size_t m_count = 0;
};

} // namespace Render::Terrain
