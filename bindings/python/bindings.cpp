// Python bindings for the AutoRemesher core engine (nanobind).
//
// Exposes the v1.1.0 C++ API:
//   - Vector3 value type with NumPy interop helpers
//   - ModelType enum
//   - AutoRemesher class (triangle mesh -> quad-dominant mesh)
//
// remesh() releases the GIL so the TBB worker threads run freely; the
// progress callback re-acquires it per call.

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <AutoRemesher/AutoRemesher>
#include <AutoRemesher/Vector3>

#include <cmath>
#include <string>
#include <unordered_map>

namespace nb = nanobind;
using AutoRemesher::Vector3;
// NB: "AutoRemesher" names both the namespace and the main class, so no
// using-declaration for it is possible; use AutoRemesher::AutoRemesher.

namespace {

// Progress callbacks are registered per AutoRemesher instance; the C++ core
// stores a raw function pointer + void* tag, so the registry maps the tag
// (the instance address) back to the Python callable. The registry is
// NON-OWNING: the binding uses nb::keep_alive<1, 2> so the callable is owned
// by the AutoRemesher Python instance and dies with it, which also means the
// C++ object (owned by that same instance) can never fire the trampoline
// after the callable is gone.
std::unordered_map<void*, PyObject*> g_progressCallbacks;

void progressTrampoline(void* tag, float progress, const char* status)
{
    auto it = g_progressCallbacks.find(tag);
    if (it == g_progressCallbacks.end() || nullptr == it->second)
        return;
    nb::gil_scoped_acquire gil;
    nb::borrow<nb::callable>(it->second)(progress, std::string(status ? status : ""));
}

void setProgressCallback(AutoRemesher::AutoRemesher& self, nb::object callback)
{
    if (callback.is_none()) {
        g_progressCallbacks.erase(&self);
        self.setProgressHandler(nullptr);
        self.setTag(nullptr);
        return;
    }
    g_progressCallbacks[&self] = callback.ptr();
    self.setTag(&self);
    self.setProgressHandler(progressTrampoline);
}

std::vector<Vector3> verticesFromNumpy(
    nb::ndarray<double, nb::shape<-1, 3>, nb::c_contig> array)
{
    std::vector<Vector3> vertices;
    vertices.reserve(array.shape(0));
    const double* data = array.data();
    for (size_t i = 0; i < array.shape(0); ++i)
        vertices.emplace_back(data[3 * i], data[3 * i + 1], data[3 * i + 2]);
    return vertices;
}

std::vector<std::vector<size_t>> facesFromNumpy(
    nb::ndarray<size_t, nb::shape<-1, -1>, nb::c_contig> array)
{
    const size_t rows = array.shape(0);
    const size_t cols = array.shape(1);
    std::vector<std::vector<size_t>> faces;
    faces.reserve(rows);
    const size_t* data = array.data();
    for (size_t i = 0; i < rows; ++i) {
        std::vector<size_t> face(cols);
        for (size_t j = 0; j < cols; ++j)
            face[j] = data[i * cols + j];
        faces.push_back(std::move(face));
    }
    return faces;
}

nb::ndarray<nb::numpy, double> verticesToNumpy(const std::vector<Vector3>& vertices)
{
    const size_t n = vertices.size();
    double* data = new double[n * 3];
    for (size_t i = 0; i < n; ++i) {
        data[3 * i + 0] = vertices[i].x();
        data[3 * i + 1] = vertices[i].y();
        data[3 * i + 2] = vertices[i].z();
    }
    nb::capsule owner(data, [](void* p) noexcept { delete[] (double*)p; });
    size_t shape[2] = { n, 3 };
    return nb::ndarray<nb::numpy, double>(data, 2, shape, owner);
}

} // namespace

NB_MODULE(autremesh, m)
{
    m.doc() = "Python bindings for the AutoRemesher core engine (v1.1.0)";

    nb::class_<Vector3>(m, "Vector3")
        .def(nb::init<>())
        .def(nb::init<double, double, double>(), nb::arg("x"), nb::arg("y"), nb::arg("z"))
        .def_prop_rw("x", &Vector3::x, &Vector3::setX)
        .def_prop_rw("y", &Vector3::y, &Vector3::setY)
        .def_prop_rw("z", &Vector3::z, &Vector3::setZ)
        .def("length", &Vector3::length)
        .def("length_squared", &Vector3::lengthSquared)
        .def("normalized", &Vector3::normalized)
        .def("normalize", &Vector3::normalize)
        .def("is_zero", &Vector3::isZero)
        .def("__getitem__", [](const Vector3& v, size_t i) {
            if (i >= 3)
                throw nb::index_error();
            return v[i];
        })
        .def("__setitem__", [](Vector3& v, size_t i, double value) {
            if (i >= 3)
                throw nb::index_error();
            v[i] = value;
        })
        .def("__repr__", [](const Vector3& v) {
            return "<Vector3(" + std::to_string(v.x()) + ", "
                + std::to_string(v.y()) + ", " + std::to_string(v.z()) + ")>";
        })
        .def("__add__", [](const Vector3& a, const Vector3& b) { return a + b; }, nb::is_operator())
        .def("__sub__", [](const Vector3& a, const Vector3& b) { return a - b; }, nb::is_operator())
        .def("__mul__", [](const Vector3& v, double s) { return v * s; }, nb::is_operator())
        .def("__rmul__", [](const Vector3& v, double s) { return s * v; }, nb::is_operator())
        .def("__truediv__", [](const Vector3& v, double s) { return v / s; }, nb::is_operator())
        .def("__neg__", [](const Vector3& v) { return -v; }, nb::is_operator())
        .def_static("dot_product", &Vector3::dotProduct)
        .def_static("cross_product", &Vector3::crossProduct)
        .def_static("normal", &Vector3::normal)
        .def_static("area", &Vector3::area)
        .def_static("angle", &Vector3::angle);

    nb::enum_<AutoRemesher::ModelType>(m, "ModelType")
        .value("Organic", AutoRemesher::ModelType::Organic)
        .value("HardSurface", AutoRemesher::ModelType::HardSurface)
        .export_values();

    nb::class_<AutoRemesher::AutoRemesher>(m, "AutoRemesher")
        .def(nb::init<const std::vector<Vector3>&, const std::vector<std::vector<size_t>>&>(),
            nb::arg("vertices"), nb::arg("triangles"),
            "Initialize AutoRemesher with vertices and triangle faces")
        .def("set_target_triangle_count", &AutoRemesher::AutoRemesher::setTargetTriangleCount,
            nb::arg("count"),
            "Set the target triangle count after uniform remeshing (quads * 2)")
        .def("set_scaling", &AutoRemesher::AutoRemesher::setScaling,
            nb::arg("scaling"), "Set the edge scaling factor (1.0-4.0 typically)")
        .def("set_model_type", &AutoRemesher::AutoRemesher::setModelType,
            nb::arg("model_type"), "Set the model type (Organic or HardSurface)")
        .def("set_gradient_adaptivity", &AutoRemesher::AutoRemesher::setGradientAdaptivity,
            nb::arg("adaptivity"), "Curvature-adaptive quad density (0.0-1.0)")
        .def("set_sharp_edge_degrees", &AutoRemesher::AutoRemesher::setSharpEdgeDegrees,
            nb::arg("degrees"), "Sharp edge dihedral angle threshold in degrees")
        .def("set_anisotropy", &AutoRemesher::AutoRemesher::setAnisotropy,
            nb::arg("anisotropy"), "Curvature-adaptive quad elongation (0.0-1.0)")
        .def("set_smooth_normal_degrees", &AutoRemesher::AutoRemesher::setSmoothNormalDegrees,
            nb::arg("degrees"), "Smooth normal angle threshold in degrees")
        .def("set_progress_callback", &setProgressCallback,
            nb::arg("callback").none(), nb::keep_alive<1, 2>(),
            "Set a progress callback fn(progress: float, status: str); None to clear")
        .def("remesh", &AutoRemesher::AutoRemesher::remesh,
            nb::call_guard<nb::gil_scoped_release>(),
            "Perform the remeshing operation (releases the GIL)")
        .def("get_remeshed_vertices", &AutoRemesher::AutoRemesher::remeshedVertices,
            nb::rv_policy::reference_internal,
            "Get the remeshed vertices")
        .def("get_remeshed_quads", &AutoRemesher::AutoRemesher::remeshedQuads,
            nb::rv_policy::reference_internal,
            "Get the remeshed faces (mostly quads, some triangles)")
        .def("phase_report", &AutoRemesher::AutoRemesher::phaseReport,
            nb::rv_policy::reference_internal,
            "Per-phase timing lines collected during the last remesh() call");

    // NumPy interop helpers
    m.def("vertices_from_numpy", &verticesFromNumpy, nb::arg("array"),
        "Convert an (N,3) float64 numpy array to a list of Vector3");
    m.def("vertices_to_numpy", &verticesToNumpy, nb::arg("vertices"),
        "Convert a list of Vector3 to an (N,3) float64 numpy array");
    m.def("faces_from_numpy", &facesFromNumpy, nb::arg("array"),
        "Convert an (N,3) or (N,4) integer numpy array to a list of faces");

    m.attr("__version__") = "1.1.0";
}
