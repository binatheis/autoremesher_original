#pragma once

// Procedural test meshes: no external assets needed.

#include <AutoRemesher/Vector3>
#include <cstddef>
#include <vector>

namespace TestMeshes {

struct Mesh {
    std::vector<AutoRemesher::Vector3> vertices;
    std::vector<std::vector<size_t>> triangles;
};

// Axis-aligned unit cube: 8 vertices, 12 triangles.
inline Mesh cube(double ox = 0.0, double oy = 0.0, double oz = 0.0)
{
    Mesh mesh;
    for (double x : { 0.0, 1.0 })
        for (double y : { 0.0, 1.0 })
            for (double z : { 0.0, 1.0 })
                mesh.vertices.emplace_back(x + ox, y + oy, z + oz);
    // 6 quad faces, fan-triangulated.
    const size_t quads[6][4] = {
        { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 },
        { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 }
    };
    for (const auto& q : quads) {
        mesh.triangles.push_back({ q[0], q[1], q[2] });
        mesh.triangles.push_back({ q[0], q[2], q[3] });
    }
    return mesh;
}

// Two cubes far apart: exercises the multi-island (parallel) code path.
inline Mesh twoCubes()
{
    Mesh a = cube(0.0, 0.0, 0.0);
    Mesh b = cube(10.0, 0.0, 0.0);
    Mesh merged = a;
    const size_t offset = a.vertices.size();
    merged.vertices.insert(merged.vertices.end(), b.vertices.begin(), b.vertices.end());
    for (const auto& tri : b.triangles)
        merged.triangles.push_back({ tri[0] + offset, tri[1] + offset, tri[2] + offset });
    return merged;
}

// A grid in the XY plane (open surface with a boundary).
inline Mesh grid(size_t resolution = 4)
{
    Mesh mesh;
    const size_t n = resolution + 1;
    for (size_t y = 0; y < n; ++y)
        for (size_t x = 0; x < n; ++x)
            mesh.vertices.emplace_back((double)x, (double)y, 0.0);
    for (size_t y = 0; y < resolution; ++y) {
        for (size_t x = 0; x < resolution; ++x) {
            const size_t v0 = y * n + x;
            const size_t v1 = v0 + 1;
            const size_t v2 = v0 + n;
            const size_t v3 = v2 + 1;
            mesh.triangles.push_back({ v0, v1, v3 });
            mesh.triangles.push_back({ v0, v3, v2 });
        }
    }
    return mesh;
}

} // namespace TestMeshes
