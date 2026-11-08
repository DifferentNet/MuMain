#pragma once

class BMD;
struct _Mesh_t;

// The Remaster's smoother models: when a model loads, each of its triangles is split into four and
// the new points on the edges are lifted onto the curve the normals describe (Phong tessellation),
// so round shapes (shoulders, helmets, bodies, wheels) lose their angular outline. Hard edges stay
// sharp: an edge whose two sides have different normals, or whose normals turn away from each other
// steeply, keeps its straight midpoint. The original points, their indices and the textures stay as
// they are; only new points and triangles are added. Each new point follows the bone of one end of
// its edge.
namespace Render::Models::Smoothing
{

// Levels of splitting for the models loaded from now on: 0 = off (Classic), 1 = four triangles for
// one, 2 = sixteen. Meshes that would grow past the renderer's limits are split less or left as they
// are.
void SetLevel(int level);
[[nodiscard]] int Level();

// Smooths every mesh of a freshly opened model (before Init); keeps the original meshes so
// RestoreMesh can bring one back.
void Apply(BMD& model);

// Gives a smoothed mesh its original points and triangles back, for the code that reads a mesh as a
// grid of its own (the cloth meshes). Does nothing for a mesh that was not smoothed.
void RestoreMesh(BMD& model, int mesh);

// Frees the kept original of a mesh (when the model is released).
void FreeOriginal(_Mesh_t& mesh);

} // namespace Render::Models::Smoothing
