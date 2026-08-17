"""Conan recipe for the AutoRemesher library bindings, CLI and tests.

Conan is used ONLY for the packages that this build introduces on top of the
upstream project (nanobind, CLI11, GoogleTest). The dependencies that the
upstream project already carries (Eigen, TBB, meshoptimizer, isotropicremesher)
stay bundled under thirdparty/ and are compiled by the root CMakeLists.txt.
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
