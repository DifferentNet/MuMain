#pragma once

#include "Render/Renderer/Overlay2DRecorder.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/Texture.h>

#include <span>

namespace Render::RmlUi
{
// The element <additive-mesh>: textured quads whose colour is added to what is behind them, like the native
// BlendMode::Additive (Render::Renderer::RecordedAdditiveMesh). RmlUi blends premultiplied (ONE, ONE_MINUS_SRC_ALPHA)
// and premultiplies the file on load, so the vertices carry the colour with zero alpha. The world labels draw the
// title plates' effects with it (UI::Character::WorldLabelLayer).
class AdditiveMeshElement final : public Rml::Element
{
public:
    explicit AdditiveMeshElement(const Rml::String& tag);

    // source: the image as an <img> src names it; quads: four vertices each, positions relative to the element's
    // border box; their colours are multiplied by opacity.
    void SetMesh(const Rml::String& source, std::span<const Render::Renderer::RecordedVertex> quads, float opacity);

protected:
    void OnRender() override;

private:
    Rml::String m_source;
    Rml::Texture m_texture;
    Rml::Mesh m_mesh;
    Rml::Geometry m_geometry;
    bool m_textureDirty = false;
    bool m_meshDirty = false;
};

// Makes <additive-mesh> an AdditiveMeshElement. Call once after Rml::Initialise().
void RegisterAdditiveMeshElement();
} // namespace Render::RmlUi
