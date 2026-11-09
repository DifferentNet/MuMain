#include "stdafx.h"
#include "Render/Models/ModelSmoothing.h"

#include "Render/Models/ZzzBMD.h"
#include "Render/Renderer/MuRenderer.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

// The original arrays of a smoothed mesh (Mesh_t::SmoothOriginal).
struct MeshSmoothOriginal
{
    short NumVertices = 0;
    short NumNormals = 0;
    short NumTexCoords = 0;
    short NumTriangles = 0;
    Vertex_t* Vertices = nullptr;
    Normal_t* Normals = nullptr;
    TexCoord_t* TexCoords = nullptr;
    Triangle_t* Triangles = nullptr;
};

namespace Render::Models::Smoothing
{
namespace
{
int g_level = 0;

// How far a midpoint moves towards the curve (Phong tessellation's alpha).
constexpr float Bulge = 0.75f;
// Normals of the two ends of an edge closer than this (cosine) make a smooth edge; steeper is a
// hard edge (blades, spikes, box corners).
constexpr float SmoothCosine = 0.55f;
// Normals of one point on the two sides of an edge differ less than this (cosine) on a smooth edge.
constexpr float SameNormalCosine = 0.98f;
// The renderer's limits for one mesh (ZzzBMD.h: MAX_VERTICES, the triangle arrays of MAX_VERTICES * 3).
constexpr int MaxPoints = MAX_VERTICES;
constexpr int MaxTriangles = MAX_VERTICES;

struct Vec
{
    float x, y, z;
};

Vec Add(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec Sub(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec Mul(Vec a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec Normalize(Vec a)
{
    const float length = std::sqrt(Dot(a, a));
    return length > 1e-6f ? Mul(a, 1.0f / length) : Vec{0.0f, 0.0f, 1.0f};
}

// Bone matrices of the model's first animation frame: the pose the mesh is drawn in when it stands.
bool BindPose(const BMD& model, std::vector<std::array<std::array<float, 4>, 3>>& out)
{
    out.assign(std::max<int>(model.NumBones, 1), {});
    for (auto& m : out)
    {
        m = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
    }
    for (int i = 0; i < model.NumBones; ++i)
    {
        const Bone_t& bone = model.Bones[i];
        if (bone.Dummy)
            continue;
        if (model.NumActions <= 0 || model.Actions[0].NumAnimationKeys <= 0 || bone.BoneMatrixes == nullptr ||
            bone.BoneMatrixes[0].Quaternion == nullptr || bone.BoneMatrixes[0].Position == nullptr)
            return false;
        float local[3][4];
        QuaternionMatrix(bone.BoneMatrixes[0].Quaternion[0], local);
        local[0][3] = bone.BoneMatrixes[0].Position[0][0];
        local[1][3] = bone.BoneMatrixes[0].Position[0][1];
        local[2][3] = bone.BoneMatrixes[0].Position[0][2];
        float world[3][4];
        if (bone.Parent >= 0 && bone.Parent < i)
        {
            float parent[3][4];
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 4; ++c)
                    parent[r][c] = out[bone.Parent][r][c];
            R_ConcatTransforms(parent, local, world);
        }
        else
        {
            std::memcpy(world, local, sizeof(world));
        }
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                out[i][r][c] = world[r][c];
    }
    return true;
}

struct Pose
{
    std::vector<std::array<std::array<float, 4>, 3>> bones;

    void Get(int node, float m[3][4]) const
    {
        const auto& b = bones[(node >= 0 && node < static_cast<int>(bones.size())) ? node : 0];
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                m[r][c] = b[r][c];
    }
    Vec ToWorld(int node, const vec3_t p) const
    {
        float m[3][4];
        Get(node, m);
        vec3_t o;
        VectorTransform(p, m, o);
        return {o[0], o[1], o[2]};
    }
    Vec NormalToWorld(int node, const vec3_t n) const
    {
        float m[3][4];
        Get(node, m);
        vec3_t o;
        VectorRotate(n, m, o);
        return Normalize({o[0], o[1], o[2]});
    }
    void ToLocal(int node, Vec p, vec3_t out) const
    {
        float m[3][4];
        Get(node, m);
        const vec3_t d = {p.x - m[0][3], p.y - m[1][3], p.z - m[2][3]};
        VectorIRotate(d, m, out);
    }
    void NormalToLocal(int node, Vec n, vec3_t out) const
    {
        float m[3][4];
        Get(node, m);
        const vec3_t d = {n.x, n.y, n.z};
        VectorIRotate(d, m, out);
    }
};

std::uint32_t Key(int a, int b)
{
    if (a > b)
        std::swap(a, b);
    return (static_cast<std::uint32_t>(a) << 16) | static_cast<std::uint32_t>(b & 0xFFFF);
}

// One level of splitting. Returns false (and leaves the mesh as it is) when the mesh can not be split.
bool SplitOnce(Mesh_t& m, const Pose& pose)
{
    const int vertexCount = m.NumVertices;
    const int normalCount = m.NumNormals;
    const int texCount = m.NumTexCoords;
    const int triangleCount = m.NumTriangles;
    if (triangleCount <= 0 || vertexCount <= 0 || normalCount <= 0 || texCount <= 0)
        return false;
    if (triangleCount * 4 > MaxTriangles)
        return false;
    for (int t = 0; t < triangleCount; ++t)
    {
        const Triangle_t& tri = m.Triangles[t];
        if (tri.Polygon != 3)
            return false;
        for (int k = 0; k < 3; ++k)
        {
            if (tri.VertexIndex[k] < 0 || tri.VertexIndex[k] >= vertexCount || tri.NormalIndex[k] < 0 ||
                tri.NormalIndex[k] >= normalCount || tri.TexCoordIndex[k] < 0 || tri.TexCoordIndex[k] >= texCount)
                return false;
        }
    }

    std::vector<Vec> position(vertexCount);
    for (int i = 0; i < vertexCount; ++i)
        position[i] = pose.ToWorld(m.Vertices[i].Node, m.Vertices[i].Position);
    std::vector<Vec> normal(normalCount);
    for (int i = 0; i < normalCount; ++i)
        normal[i] = pose.NormalToWorld(m.Normals[i].Node, m.Normals[i].Normal);

    // Each edge (by its two points): the normals its triangles give its ends, and whether they agree.
    struct Edge
    {
        int a = -1, b = -1;
        int normalA = -1, normalB = -1;
        bool hard = false;
        int point = -1; // the new point on the edge; -1: the edge is not split
    };
    std::unordered_map<std::uint32_t, Edge> edges;
    edges.reserve(triangleCount * 2);
    for (int t = 0; t < triangleCount; ++t)
    {
        const Triangle_t& tri = m.Triangles[t];
        for (int k = 0; k < 3; ++k)
        {
            const int a = tri.VertexIndex[k];
            const int b = tri.VertexIndex[(k + 1) % 3];
            const int na = tri.NormalIndex[k];
            const int nb = tri.NormalIndex[(k + 1) % 3];
            Edge& e = edges[Key(a, b)];
            const bool flipped = e.a >= 0 && e.a != a;
            const int ea = flipped ? b : a;
            const int eb = flipped ? a : b;
            const int ena = flipped ? nb : na;
            const int enb = flipped ? na : nb;
            if (e.a < 0)
            {
                e.a = ea;
                e.b = eb;
                e.normalA = ena;
                e.normalB = enb;
                e.hard = Dot(normal[ena], normal[enb]) < SmoothCosine;
            }
            else if (!e.hard)
            {
                if (Dot(normal[e.normalA], normal[ena]) < SameNormalCosine ||
                    Dot(normal[e.normalB], normal[enb]) < SameNormalCosine)
                    e.hard = true;
            }
        }
    }

    // An edge between points of two bones is not split: its new point could follow only one of them,
    // so the bend would break when the bones turn apart. Its triangles are split into two or three.
    int newPoints = 0;
    for (const auto& [key, e] : edges)
    {
        if (m.Vertices[e.a].Node == m.Vertices[e.b].Node)
            ++newPoints;
    }
    if (newPoints == 0)
        return false;
    if (vertexCount + newPoints > MaxPoints || vertexCount + newPoints > SHRT_MAX)
        return false;

    // New normals and texture coordinates: one for each pair of corners' entries that an edge joins.
    std::unordered_map<std::uint64_t, int> newNormal;
    std::unordered_map<std::uint32_t, int> newTex;
    std::vector<Normal_t> normals(m.Normals, m.Normals + normalCount);
    std::vector<TexCoord_t> texs(m.TexCoords, m.TexCoords + texCount);
    std::vector<Vertex_t> vertices(m.Vertices, m.Vertices + vertexCount);
    vertices.reserve(vertexCount + newPoints);

    for (auto& [key, e] : edges)
    {
        if (m.Vertices[e.a].Node != m.Vertices[e.b].Node)
            continue;
        const Vec pa = position[e.a];
        const Vec pb = position[e.b];
        Vec mid = Mul(Add(pa, pb), 0.5f);
        if (!e.hard)
        {
            const Vec na = normal[e.normalA];
            const Vec nb = normal[e.normalB];
            const Vec onA = Sub(mid, Mul(na, Dot(Sub(mid, pa), na)));
            const Vec onB = Sub(mid, Mul(nb, Dot(Sub(mid, pb), nb)));
            mid = Add(Mul(mid, 1.0f - Bulge), Mul(Add(onA, onB), 0.5f * Bulge));
        }
        Vertex_t v{};
        v.Node = m.Vertices[e.a].Node;
        pose.ToLocal(v.Node, mid, v.Position);
        e.point = static_cast<int>(vertices.size());
        vertices.push_back(v);
    }

    auto midNormal = [&](int na, int nb, int point, short node) -> int {
        // Per new point: two edges may join the same pair of normals.
        const std::uint64_t key = (static_cast<std::uint64_t>(point) << 32) | Key(na, nb);
        auto found = newNormal.find(key);
        if (found != newNormal.end())
            return found->second;
        Normal_t n{};
        n.Node = node;
        pose.NormalToLocal(node, Normalize(Add(normal[na], normal[nb])), n.Normal);
        n.BindVertex = static_cast<short>(point);
        const int index = static_cast<int>(normals.size());
        normals.push_back(n);
        newNormal.emplace(key, index);
        return index;
    };
    auto midTex = [&](int ta, int tb) -> int {
        const std::uint32_t key = Key(ta, tb);
        auto found = newTex.find(key);
        if (found != newTex.end())
            return found->second;
        TexCoord_t c{};
        c.TexCoordU = (m.TexCoords[ta].TexCoordU + m.TexCoords[tb].TexCoordU) * 0.5f;
        c.TexCoordV = (m.TexCoords[ta].TexCoordV + m.TexCoords[tb].TexCoordV) * 0.5f;
        const int index = static_cast<int>(texs.size());
        texs.push_back(c);
        newTex.emplace(key, index);
        return index;
    };

    std::vector<Triangle_t> triangles;
    triangles.reserve(triangleCount * 4);
    for (int t = 0; t < triangleCount; ++t)
    {
        const Triangle_t& tri = m.Triangles[t];
        int pv[3], pn[3], pt[3]; // the midpoints of edges 01, 12, 20; -1 where the edge is not split
        int splitEdges = 0;
        for (int k = 0; k < 3; ++k)
        {
            const int a = tri.VertexIndex[k];
            const int b = tri.VertexIndex[(k + 1) % 3];
            const Edge& e = edges[Key(a, b)];
            pv[k] = pn[k] = pt[k] = -1;
            if (e.point < 0)
                continue;
            ++splitEdges;
            pv[k] = e.point;
            pn[k] = midNormal(tri.NormalIndex[k], tri.NormalIndex[(k + 1) % 3], e.point, vertices[e.point].Node);
            pt[k] = midTex(tri.TexCoordIndex[k], tri.TexCoordIndex[(k + 1) % 3]);
        }
        if (normals.size() > static_cast<std::size_t>(MaxPoints) || texs.size() > static_cast<std::size_t>(SHRT_MAX))
            return false;

        auto make = [&](int v0, int n0, int t0, int v1, int n1, int t1, int v2, int n2, int t2) {
            Triangle_t out = tri;
            out.VertexIndex[0] = static_cast<short>(v0);
            out.VertexIndex[1] = static_cast<short>(v1);
            out.VertexIndex[2] = static_cast<short>(v2);
            out.NormalIndex[0] = static_cast<short>(n0);
            out.NormalIndex[1] = static_cast<short>(n1);
            out.NormalIndex[2] = static_cast<short>(n2);
            out.TexCoordIndex[0] = static_cast<short>(t0);
            out.TexCoordIndex[1] = static_cast<short>(t1);
            out.TexCoordIndex[2] = static_cast<short>(t2);
            for (short& edge : out.EdgeTriangleIndex)
                edge = -1;
            triangles.push_back(out);
        };
        const short* v = tri.VertexIndex;
        const short* n = tri.NormalIndex;
        const short* c = tri.TexCoordIndex;
        if (splitEdges == 3)
        {
            make(v[0], n[0], c[0], pv[0], pn[0], pt[0], pv[2], pn[2], pt[2]);
            make(pv[0], pn[0], pt[0], v[1], n[1], c[1], pv[1], pn[1], pt[1]);
            make(pv[2], pn[2], pt[2], pv[1], pn[1], pt[1], v[2], n[2], c[2]);
            make(pv[0], pn[0], pt[0], pv[1], pn[1], pt[1], pv[2], pn[2], pt[2]);
        }
        else if (splitEdges == 2)
        {
            // Corners A B C, with A-B the edge that stays whole; P on B-C, Q on C-A.
            const int a = pv[0] < 0 ? 0 : (pv[1] < 0 ? 1 : 2);
            const int b = (a + 1) % 3;
            const int cc = (a + 2) % 3;
            make(v[a], n[a], c[a], v[b], n[b], c[b], pv[b], pn[b], pt[b]);
            make(v[a], n[a], c[a], pv[b], pn[b], pt[b], pv[cc], pn[cc], pt[cc]);
            make(pv[b], pn[b], pt[b], v[cc], n[cc], c[cc], pv[cc], pn[cc], pt[cc]);
        }
        else if (splitEdges == 1)
        {
            // Corners A B C, with P on A-B, the only split edge.
            const int a = pv[0] >= 0 ? 0 : (pv[1] >= 0 ? 1 : 2);
            const int b = (a + 1) % 3;
            const int cc = (a + 2) % 3;
            make(v[a], n[a], c[a], pv[a], pn[a], pt[a], v[cc], n[cc], c[cc]);
            make(pv[a], pn[a], pt[a], v[b], n[b], c[b], v[cc], n[cc], c[cc]);
        }
        else
        {
            make(v[0], n[0], c[0], v[1], n[1], c[1], v[2], n[2], c[2]);
        }
    }

    // The renderer's arrays are indexed by normal as well (NormalTransform, g_chrome).
    if (normals.size() > static_cast<std::size_t>(MaxPoints))
        return false;

    auto* newVertices = new Vertex_t[vertices.size()];
    std::copy(vertices.begin(), vertices.end(), newVertices);
    auto* newNormals = new Normal_t[normals.size()];
    std::copy(normals.begin(), normals.end(), newNormals);
    auto* newTexs = new TexCoord_t[texs.size()];
    std::copy(texs.begin(), texs.end(), newTexs);
    auto* newTriangles = new Triangle_t[triangles.size()];
    std::copy(triangles.begin(), triangles.end(), newTriangles);

    // The original arrays are kept by Apply on the first level; later levels free the level before.
    if (m.SmoothOriginal == nullptr || m.SmoothOriginal->Vertices != m.Vertices)
    {
        delete[] m.Vertices;
        delete[] m.Normals;
        delete[] m.TexCoords;
        delete[] m.Triangles;
    }
    m.Vertices = newVertices;
    m.Normals = newNormals;
    m.TexCoords = newTexs;
    m.Triangles = newTriangles;
    m.NumVertices = static_cast<short>(vertices.size());
    m.NumNormals = static_cast<short>(normals.size());
    m.NumTexCoords = static_cast<short>(texs.size());
    m.NumTriangles = static_cast<short>(triangles.size());
    return true;
}
} // namespace

void SetLevel(int level)
{
    g_level = std::clamp(level, 0, 2);
}

int Level()
{
    return g_level;
}

void Apply(BMD& model)
{
    if (g_level <= 0 || model.Meshs == nullptr || model.NumMeshs <= 0)
        return;
    Pose pose;
    if (!BindPose(model, pose.bones))
        return;
    for (int i = 0; i < model.NumMeshs; ++i)
    {
        Mesh_t& m = model.Meshs[i];
        auto* original = new MeshSmoothOriginal{m.NumVertices, m.NumNormals, m.NumTexCoords, m.NumTriangles,
                                                m.Vertices,    m.Normals,    m.TexCoords,    m.Triangles};
        m.SmoothOriginal = original;
        bool split = false;
        for (int level = 0; level < g_level; ++level)
        {
            if (!SplitOnce(m, pose))
                break;
            split = true;
        }
        if (!split)
        {
            m.SmoothOriginal = nullptr;
            delete original;
        }
    }
}

void RestoreMesh(BMD& model, int mesh)
{
    if (model.Meshs == nullptr || mesh < 0 || mesh >= model.NumMeshs)
        return;
    Mesh_t& m = model.Meshs[mesh];
    MeshSmoothOriginal* original = m.SmoothOriginal;
    if (original == nullptr)
        return;
    mu::GetRenderer().ReleaseSkinnedMesh(&m);
    delete[] m.Vertices;
    delete[] m.Normals;
    delete[] m.TexCoords;
    delete[] m.Triangles;
    m.Vertices = original->Vertices;
    m.Normals = original->Normals;
    m.TexCoords = original->TexCoords;
    m.Triangles = original->Triangles;
    m.NumVertices = original->NumVertices;
    m.NumNormals = original->NumNormals;
    m.NumTexCoords = original->NumTexCoords;
    m.NumTriangles = original->NumTriangles;
    m.SmoothOriginal = nullptr;
    delete original;
}

void FreeOriginal(_Mesh_t& m)
{
    MeshSmoothOriginal* original = m.SmoothOriginal;
    if (original == nullptr)
        return;
    delete[] original->Vertices;
    delete[] original->Normals;
    delete[] original->TexCoords;
    delete[] original->Triangles;
    m.SmoothOriginal = nullptr;
    delete original;
}

} // namespace Render::Models::Smoothing
