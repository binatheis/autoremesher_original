#include "obj_io.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <fstream>

namespace AutoRemesherCLI {

bool loadObj(const std::string& path,
    std::vector<AutoRemesher::Vector3>* vertices,
    std::vector<std::vector<size_t>>* triangles,
    std::string* error)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        *error = err.empty() ? "failed to parse OBJ file" : err;
        return false;
    }

    const size_t vertexCount = attrib.vertices.size() / 3;
    vertices->reserve(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        vertices->emplace_back(
            (double)attrib.vertices[3 * i + 0],
            (double)attrib.vertices[3 * i + 1],
            (double)attrib.vertices[3 * i + 2]);
    }

    for (const auto& shape : shapes) {
        const auto& mesh = shape.mesh;
        size_t indexOffset = 0;
        for (size_t f = 0; f < mesh.num_face_vertices.size(); ++f) {
            const size_t faceSize = mesh.num_face_vertices[f];
            if (faceSize < 3) {
                indexOffset += faceSize;
                continue;
            }
            // Fan triangulation for polygons with more than 3 vertices.
            const int v0 = mesh.indices[indexOffset].vertex_index;
            for (size_t k = 1; k + 1 < faceSize; ++k) {
                const int v1 = mesh.indices[indexOffset + k].vertex_index;
                const int v2 = mesh.indices[indexOffset + k + 1].vertex_index;
                if (v0 < 0 || v1 < 0 || v2 < 0)
                    continue;
                triangles->push_back({ (size_t)v0, (size_t)v1, (size_t)v2 });
            }
            indexOffset += faceSize;
        }
    }

    if (vertices->empty() || triangles->empty()) {
        *error = "OBJ file contains no triangle faces";
        return false;
    }
    return true;
}

bool saveObj(const std::string& path,
    const std::vector<AutoRemesher::Vector3>& vertices,
    const std::vector<std::vector<size_t>>& faces,
    std::string* error)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        *error = "cannot open output file for writing";
        return false;
    }

    out << "# AutoRemesher output\n";
    for (const auto& v : vertices)
        out << "v " << v.x() << " " << v.y() << " " << v.z() << "\n";
    for (const auto& face : faces) {
        if (face.size() < 3)
            continue;
        out << "f";
        for (const size_t index : face)
            out << " " << (index + 1); // OBJ indices are 1-based
        out << "\n";
    }

    if (!out.good()) {
        *error = "failed while writing output file";
        return false;
    }
    return true;
}

} // namespace AutoRemesherCLI
