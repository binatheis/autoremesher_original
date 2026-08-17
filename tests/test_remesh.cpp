#include <gtest/gtest.h>

#include <AutoRemesher/AutoRemesher>

#include "test_meshes.h"

// NB: "AutoRemesher" names both the namespace and the main class.

namespace {

// NB: remeshedVertices()/remeshedQuads() are non-const in the v1.1.0 core API.
void expectValidResult(AutoRemesher::AutoRemesher& remesher)
{
    const auto& vertices = remesher.remeshedVertices();
    const auto& faces = remesher.remeshedQuads();
    ASSERT_FALSE(vertices.empty());
    ASSERT_FALSE(faces.empty());
    size_t quads = 0;
    for (const auto& face : faces) {
        ASSERT_GE(face.size(), 3u);
        ASSERT_LE(face.size(), 8u);
        if (face.size() == 4)
            ++quads;
        for (const size_t index : face)
            ASSERT_LT(index, vertices.size());
    }
    // The result of quad remeshing must be quad-dominant.
    EXPECT_GT((double)quads / (double)faces.size(), 0.5);
}

} // namespace

TEST(RemeshTest, CubeDefaultParameters)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    ASSERT_TRUE(remesher.remesh());
    expectValidResult(remesher);
}

TEST(RemeshTest, CubeLowPolyScaling)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    remesher.setScaling(2.0);
    ASSERT_TRUE(remesher.remesh());
    expectValidResult(remesher);
}

TEST(RemeshTest, CubeNoAdaptivityNoAnisotropy)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    remesher.setGradientAdaptivity(0.0);
    remesher.setAnisotropy(0.0);
    ASSERT_TRUE(remesher.remesh());
    expectValidResult(remesher);
}

TEST(RemeshTest, CubeFullAnisotropy)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    remesher.setGradientAdaptivity(1.0);
    remesher.setAnisotropy(1.0);
    ASSERT_TRUE(remesher.remesh());
    expectValidResult(remesher);
}

TEST(RemeshTest, CubeHardSurfaceType)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    remesher.setModelType(AutoRemesher::ModelType::HardSurface);
    remesher.setSharpEdgeDegrees(60.0);
    ASSERT_TRUE(remesher.remesh());
    expectValidResult(remesher);
}

TEST(RemeshTest, OpenGridWithBoundary)
{
    // Open surfaces with boundaries must still parameterize and extract.
    const TestMeshes::Mesh mesh = TestMeshes::grid(4);
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    ASSERT_TRUE(remesher.remesh());
    const auto& faces = remesher.remeshedQuads();
    EXPECT_FALSE(faces.empty());
}

TEST(RemeshTest, PhaseReportIsPopulated)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    ASSERT_TRUE(remesher.remesh());
    const auto& report = remesher.phaseReport();
    ASSERT_FALSE(report.empty());
    bool sawTotal = false;
    for (const auto& line : report)
        if (line.find("Total:") != std::string::npos)
            sawTotal = true;
    EXPECT_TRUE(sawTotal);
}
