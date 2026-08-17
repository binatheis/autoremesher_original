"""AutoRemesher - automatic quad remeshing (triangle mesh -> quad-dominant mesh).

Python bindings for the AutoRemesher core engine, built with nanobind.

Quick start
-----------
>>> import numpy as np
>>> import autoremesher
>>> vertices, quads = autoremesher.remesh(vertices_np, triangles_np, target_quads=2000)
"""

from autoremesher.autremesh import (
    AutoRemesher,
    ModelType,
    Vector3,
    __version__,
    faces_from_numpy,
    vertices_from_numpy,
    vertices_to_numpy,
)

__all__ = [
    "AutoRemesher",
    "ModelType",
    "Vector3",
    "remesh",
    "vertices_from_numpy",
    "vertices_to_numpy",
    "faces_from_numpy",
    "__version__",
]


def remesh(vertices, triangles, target_quads=50000, edge_scaling=1.0,
           sharp_edge=90.0, smooth_normal=0.0, adaptivity=1.0,
           anisotropy=1.0, on_progress=None):
    """Remesh a triangle mesh into a quad-dominant mesh.

    Parameters
    ----------
    vertices : (N, 3) numpy.ndarray of float64, or list of Vector3
        Input mesh vertex positions.
    triangles : (M, 3) numpy.ndarray of uint64, or list of list of int
        Input mesh triangle faces (0-based indices).
    target_quads : int, default 50000
        Desired number of quads in the output (the core targets twice as
        many triangles in the uniform-remeshing stage).
    edge_scaling : float, default 1.0, range [1.0, 4.0]
        Edge length scaling. Larger values produce coarser, lower-poly
        results relative to ``target_quads``.
    sharp_edge : float, default 90.0, range [30.0, 180.0]
        Dihedral angle threshold in degrees above which an edge is treated
        as a sharp feature and aligned with the quad flow.
    smooth_normal : float, default 0.0, range [0.0, 180.0]
        Dihedral angle threshold in degrees below which normals are
        smoothed during isotropic remeshing (useful for low-poly inputs).
    adaptivity : float, default 1.0, range [0.0, 1.0]
        Curvature-adaptive density: 0.0 = uniform density, 1.0 = strongly
        concentrated on high-curvature regions.
    anisotropy : float, default 1.0, range [0.0, 1.0]
        Curvature-adaptive quad elongation: 0.0 = square quads, 1.0 =
        maximally stretched along principal curvature directions.
    on_progress : callable, optional
        ``fn(progress: float, status: str)`` invoked from worker threads
        with ``progress`` in [0.0, 1.0].

    Returns
    -------
    (vertices, faces)
        ``vertices``: (K, 3) float64 numpy array.
        ``faces``: list of lists of int (quads and a few triangles).
    """
    import numpy as np

    if isinstance(vertices, np.ndarray):
        vertices = vertices_from_numpy(np.ascontiguousarray(vertices, dtype=np.float64))
    if isinstance(triangles, np.ndarray):
        triangles = faces_from_numpy(np.ascontiguousarray(triangles, dtype=np.uint64))

    remesher = AutoRemesher(vertices, triangles)
    remesher.set_target_triangle_count(int(target_quads) * 2)
    if edge_scaling > 0.0:
        remesher.set_scaling(float(edge_scaling))
    remesher.set_model_type(ModelType.Organic)
    remesher.set_gradient_adaptivity(float(adaptivity))
    remesher.set_sharp_edge_degrees(float(sharp_edge))
    remesher.set_smooth_normal_degrees(float(smooth_normal))
    remesher.set_anisotropy(float(anisotropy))
    if on_progress is not None:
        remesher.set_progress_callback(on_progress)

    if not remesher.remesh():
        raise RuntimeError("AutoRemesher remeshing failed")

    return vertices_to_numpy(remesher.get_remeshed_vertices()), remesher.get_remeshed_quads()
