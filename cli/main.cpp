// AutoRemesher standalone CLI (Qt-free).
//
// Mirrors the headless mode of the upstream GUI application:
//   autoremesher --input in.obj --output out.obj [--report r.txt]
//                [--target-quads 50000] [--edge-scaling 1.0]
//                [--sharp-edge 90.0] [--smooth-normal 0.0]
//                [--adaptivity 1.0] [--anisotropy 1.0]

#include <CLI/CLI.hpp>

#include <AutoRemesher/AutoRemesher>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "obj_io.h"

namespace {

struct CliParams {
    std::string input;
    std::string output;
    std::string report;
    int targetQuads = 50000;
    double edgeScaling = 1.0;
    double sharpEdgeDegrees = 90.0;
    double smoothNormalDegrees = 0.0;
    double adaptivity = 1.0;
    double anisotropy = 1.0;
    bool quiet = false;
};

int g_lastPermille = -1;
void printProgress(void*, float progress, const char* status)
{
    const int permille = (int)(progress * 1000.0f);
    if (permille == g_lastPermille)
        return;
    g_lastPermille = permille;
    std::printf("\r%5.1f%%  %-40s", progress * 100.0f, status ? status : "");
    std::fflush(stdout);
    if (permille >= 1000)
        std::printf("\n");
}

size_t countQuads(const std::vector<std::vector<size_t>>& faces)
{
    size_t quads = 0;
    for (const auto& face : faces)
        if (face.size() == 4)
            ++quads;
    return quads;
}

int runRemesh(const CliParams& params)
{
    std::vector<AutoRemesher::Vector3> vertices;
    std::vector<std::vector<size_t>> triangles;
    std::string error;
    if (!AutoRemesherCLI::loadObj(params.input, &vertices, &triangles, &error)) {
        std::cerr << "Error loading " << params.input << ": " << error << std::endl;
        return 2;
    }
    if (!params.quiet) {
        std::cout << "Input: " << params.input << " (" << vertices.size()
                  << " vertices, " << triangles.size() << " triangles)" << std::endl;
    }

    AutoRemesher::AutoRemesher remesher(vertices, triangles);
    // Same mapping as the upstream GUI: quads -> triangles x2.
    remesher.setTargetTriangleCount((size_t)params.targetQuads * 2);
    if (params.edgeScaling > 0.0)
        remesher.setScaling(params.edgeScaling);
    remesher.setModelType(AutoRemesher::ModelType::Organic);
    remesher.setGradientAdaptivity(params.adaptivity);
    remesher.setAnisotropy(params.anisotropy);
    remesher.setSharpEdgeDegrees(params.sharpEdgeDegrees);
    remesher.setSmoothNormalDegrees(params.smoothNormalDegrees);
    if (!params.quiet)
        remesher.setProgressHandler(printProgress);

    const auto t0 = std::chrono::high_resolution_clock::now();
    const bool success = remesher.remesh();
    const auto t1 = std::chrono::high_resolution_clock::now();
    const double elapsed = std::chrono::duration<double>(t1 - t0).count();

    if (!success) {
        std::cerr << "Remeshing failed" << std::endl;
        return 3;
    }

    const auto& outVertices = remesher.remeshedVertices();
    const auto& outFaces = remesher.remeshedQuads();
    const size_t quads = countQuads(outFaces);
    const size_t nonQuads = outFaces.size() - quads;

    if (!AutoRemesherCLI::saveObj(params.output, outVertices, outFaces, &error)) {
        std::cerr << "Error writing " << params.output << ": " << error << std::endl;
        return 2;
    }

    if (!params.quiet) {
        std::cout << "=== AutoRemesher Report ===" << std::endl;
        std::cout << "Input: " << params.input << std::endl;
        std::cout << "Output: " << params.output << std::endl;
        std::cout << "Quads: " << quads << std::endl;
        std::cout << "Non-quads: " << nonQuads << std::endl;
        std::cout << "Vertices: " << outVertices.size() << std::endl;
        std::cout << "Time: " << elapsed << " seconds" << std::endl;
        std::cout << "===========================" << std::endl;
        for (const auto& line : remesher.phaseReport())
            std::cout << line << std::endl;
    }

    if (!params.report.empty()) {
        std::ofstream report(params.report, std::ios::out | std::ios::trunc);
        if (report.is_open()) {
            report << "AutoRemesher Report\n"
                   << "===================\n\n"
                   << "Input file: " << params.input << "\n"
                   << "Output file: " << params.output << "\n"
                   << "Target quads: " << params.targetQuads << "\n"
                   << "Edge scaling: " << params.edgeScaling << "\n"
                   << "Sharp edge degrees: " << params.sharpEdgeDegrees << "\n"
                   << "Smooth normal degrees: " << params.smoothNormalDegrees << "\n"
                   << "Adaptivity: " << params.adaptivity << "\n"
                   << "Anisotropy: " << params.anisotropy << "\n\n"
                   << "Results:\n"
                   << "  Quads: " << quads << "\n"
                   << "  Non-quads: " << nonQuads << "\n"
                   << "  Vertices: " << outVertices.size() << "\n"
                   << "  Total time: " << elapsed << " seconds\n\n"
                   << "Phase timings:\n";
            for (const auto& line : remesher.phaseReport())
                report << "  " << line << "\n";
        } else {
            std::cerr << "Warning: could not write report file " << params.report << std::endl;
        }
    }

    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    CliParams params;

    CLI::App app { "AutoRemesher CLI - automatic quad remeshing (triangle mesh -> quad-dominant mesh)" };
    app.footer("Upstream project: https://github.com/huxingyi/autoremesher");

    app.add_option("-i,--input", params.input, "Input .obj file to remesh")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-o,--output", params.output, "Output .obj file path for the remeshed result")
        ->required();
    app.add_option("--report", params.report,
        "Path to write a report file with stats (quads, non-quads, vertices, phase timings)");
    app.add_option("--target-quads", params.targetQuads,
        "Target quad count (default: 50000)")
        ->check(CLI::PositiveNumber);
    app.add_option("--edge-scaling", params.edgeScaling,
        "Edge scaling factor (default: 1.0, range: 1.0-4.0)")
        ->check(CLI::Range(1.0, 4.0));
    app.add_option("--sharp-edge", params.sharpEdgeDegrees,
        "Sharp edge dihedral angle threshold in degrees (default: 90.0, range: 30.0-180.0)")
        ->check(CLI::Range(30.0, 180.0));
    app.add_option("--smooth-normal", params.smoothNormalDegrees,
        "Smooth normal angle threshold in degrees (default: 0.0, range: 0.0-180.0)")
        ->check(CLI::Range(0.0, 180.0));
    app.add_option("--adaptivity", params.adaptivity,
        "Curvature-adaptive quad density (default: 1.0, range: 0.0-1.0)")
        ->check(CLI::Range(0.0, 1.0));
    app.add_option("--anisotropy", params.anisotropy,
        "Curvature-adaptive quad elongation (default: 1.0, range: 0.0-1.0)")
        ->check(CLI::Range(0.0, 1.0));
    app.add_flag("-q,--quiet", params.quiet, "Suppress progress and summary output");

    CLI11_PARSE(app, argc, argv);
    return runRemesh(params);
}
