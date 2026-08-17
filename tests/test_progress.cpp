#include <gtest/gtest.h>

#include <AutoRemesher/AutoRemesher>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "test_meshes.h"

// NB: "AutoRemesher" names both the namespace and the main class.

namespace {

struct ProgressRecorder {
    std::vector<float> values;
    std::vector<std::string> statuses;
    std::mutex mutex;
};

void recordProgress(void* tag, float progress, const char* status)
{
    ProgressRecorder* recorder = static_cast<ProgressRecorder*>(tag);
    std::lock_guard<std::mutex> lock(recorder->mutex);
    recorder->values.push_back(progress);
    recorder->statuses.emplace_back(status ? status : "");
}

} // namespace

TEST(ProgressTest, ReportsMonotonicProgressAndCompletes)
{
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);

    ProgressRecorder recorder;
    remesher.setTag(&recorder);
    remesher.setProgressHandler(recordProgress);

    ASSERT_TRUE(remesher.remesh());

    ASSERT_FALSE(recorder.values.empty());
    EXPECT_DOUBLE_EQ(recorder.values.front(), 0.0);
    for (size_t i = 1; i < recorder.values.size(); ++i)
        EXPECT_GE(recorder.values[i], recorder.values[i - 1] - 1e-6f)
            << "progress regressed at call " << i;
    EXPECT_DOUBLE_EQ(recorder.values.back(), 1.0);
    EXPECT_EQ(recorder.statuses.back(), "Done");
}

TEST(ProgressTest, WorksWithoutHandler)
{
    // No handler installed: remesh must still succeed and not crash.
    const TestMeshes::Mesh mesh = TestMeshes::cube();
    AutoRemesher::AutoRemesher remesher(mesh.vertices, mesh.triangles);
    remesher.setTargetTriangleCount(200);
    ASSERT_TRUE(remesher.remesh());
}

TEST(ProgressTest, TwoDisconnectedCubes)
{
    // Exercises MeshSeparator + the multi-island parallel path: both islands
    // must contribute to the merged result.
    const TestMeshes::Mesh single = TestMeshes::cube();
    const TestMeshes::Mesh dual = TestMeshes::twoCubes();

    AutoRemesher::AutoRemesher remesherSingle(single.vertices, single.triangles);
    remesherSingle.setTargetTriangleCount(200);
    ASSERT_TRUE(remesherSingle.remesh());

    AutoRemesher::AutoRemesher remesherDual(dual.vertices, dual.triangles);
    remesherDual.setTargetTriangleCount(400);

    ProgressRecorder recorder;
    remesherDual.setTag(&recorder);
    remesherDual.setProgressHandler(recordProgress);
    ASSERT_TRUE(remesherDual.remesh());

    // Two islands should produce roughly twice as much geometry as one.
    EXPECT_GT(remesherDual.remeshedQuads().size(),
        remesherSingle.remeshedQuads().size());
    ASSERT_FALSE(recorder.values.empty());
    EXPECT_DOUBLE_EQ(recorder.values.back(), 1.0);
}
