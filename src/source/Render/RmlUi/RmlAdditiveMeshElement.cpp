#include "Render/RmlUi/RmlAdditiveMeshElement.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/RenderManager.h>
#include <RmlUi/Core/URL.h>

#include <cmath>
#include <cstdint>
#include <utility>

namespace
{
constexpr std::size_t QuadVertices = 4;

Rml::byte Channel(std::uint32_t abgr, int shift, float opacity)
{
    return static_cast<Rml::byte>(std::lround(static_cast<float>((abgr >> shift) & 0xFFu) * opacity));
}

// The colour added: RGB times opacity, zero alpha (see the header).
Rml::ColourbPremultiplied Additive(std::uint32_t abgr, float opacity)
{
    return Rml::ColourbPremultiplied(Channel(abgr, 0, opacity), Channel(abgr, 8, opacity), Channel(abgr, 16, opacity),
                                     0);
}
} // namespace

Render::RmlUi::AdditiveMeshElement::AdditiveMeshElement(const Rml::String& tag) : Rml::Element(tag) {}

void Render::RmlUi::AdditiveMeshElement::SetMesh(const Rml::String& source,
                                                 std::span<const Render::Renderer::RecordedVertex> quads,
                                                 float opacity)
{
    if (m_source != source)
    {
        m_source = source;
        m_textureDirty = true;
    }

    m_mesh.vertices.clear();
    m_mesh.indices.clear();
    const std::size_t quadCount = quads.size() / QuadVertices;
    for (std::size_t q = 0; q < quadCount; ++q)
    {
        const int first = static_cast<int>(m_mesh.vertices.size());
        for (std::size_t i = 0; i < QuadVertices; ++i)
        {
            const Render::Renderer::RecordedVertex& v = quads[q * QuadVertices + i];
            m_mesh.vertices.push_back({Rml::Vector2f(v.x, v.y), Additive(v.abgr, opacity), Rml::Vector2f(v.u, v.v)});
        }
        for (const int index : {0, 1, 2, 0, 2, 3})
            m_mesh.indices.push_back(first + index);
    }
    m_meshDirty = true;
}

void Render::RmlUi::AdditiveMeshElement::OnRender()
{
    Rml::RenderManager* renderManager = GetRenderManager();
    if (renderManager == nullptr || m_source.empty())
        return;

    if (m_textureDirty)
    {
        m_textureDirty = false;
        // Resolved as an <img> src: against the document's path.
        Rml::URL documentUrl;
        if (Rml::ElementDocument* document = GetOwnerDocument())
            documentUrl.SetURL(document->GetSourceURL());
        m_texture = renderManager->LoadTexture(m_source, documentUrl.GetPath());
    }
    if (m_meshDirty)
    {
        m_meshDirty = false;
        // MakeGeometry takes the mesh; SetMesh() fills it again next frame.
        Rml::Mesh mesh = m_mesh;
        m_geometry = renderManager->MakeGeometry(std::move(mesh));
    }
    if (m_texture && m_geometry)
        m_geometry.Render(GetAbsoluteOffset(Rml::BoxArea::Border), m_texture);
}

void Render::RmlUi::RegisterAdditiveMeshElement()
{
    static Rml::ElementInstancerGeneric<AdditiveMeshElement> instancer;
    Rml::Factory::RegisterElementInstancer("additive-mesh", &instancer);
}
