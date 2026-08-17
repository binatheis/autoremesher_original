"""Conan recipe for the AutoRemesher library bindings, CLI and tests.

Conan is used ONLY for the packages that this build introduces on top of the
upstream project (nanobind, CLI11, GoogleTest). The dependencies that the
upstream project already carries (Eigen, meshoptimizer, isotropicremesher)
stay bundled under thirdparty/ and are compiled by the root CMakeLists.txt.

On Linux the bundled legacy TBB (circa 2016) crashes when statically linked
into a Python extension .so because its RML runtime loader is not designed
for that context. We pull oneTBB from Conan on Linux only; Windows/macOS
keep the bundled static TBB which works fine there.
"""

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class AutoRemesherConan(ConanFile):
    name = "autoremesher"
    version = "1.1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("nanobind/2.13.0")
        self.requires("cli11/2.2.0")
        self.requires("gtest/1.18.0")
        if self.settings.os == "Linux":
            # oneTBB 2022.0.0 is the first version that builds with GCC 14
            # (the compiler in the manylinux_2_28 container). Only the core
            # tbb library is needed; skip tbbmalloc/tbbproxy/tbbbind.
            self.requires("onetbb/2022.0.0",
                          options={"tbbmalloc": False, "tbbproxy": False, "tbbbind": False})

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        # The nanobind Conan package does not ship a CMakeDeps-generated
        # config file; it carries upstream's own sources and cmake modules
        # inside <pkg>/nanobind/. Expose the package folder so the root
        # CMakeLists.txt can compile nb_combined.cpp per Python target.
        nanobind_root = self.dependencies["nanobind"].package_folder
        tc.variables["NANOBIND_ROOT"] = nanobind_root.replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
