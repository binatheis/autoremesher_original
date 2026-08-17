#pragma once

#include <AutoRemesher/Vector3>
#include <cstddef>
#include <string>
#include <vector>

namespace AutoRemesherCLI {

// Loads an OBJ file as a triangle soup. Faces with more than three vertices
// are fan-triangulated. Returns false and fills `error` on failure.
bool loadObj(const std::string& path,
    std::vector<AutoRemesher::Vector3>* vertices,
    std::vector<std::vector<size_t>>* triangles,
    std::string* error);

// Writes vertices and polygons (quads and triangles) in OBJ format.
bool saveObj(const std::string& path,
    const std::vector<AutoRemesher::Vector3>& vertices,
    const std::vector<std::vector<size_t>>& faces,
    std::string* error);

} // namespace AutoRemesherCLI
