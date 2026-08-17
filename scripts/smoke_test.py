#!/usr/bin/env python3
"""Smoke test for an installed autoremesher wheel.

Remeshes a procedural cube through the public package API and checks the
result. Used by CI after `pip install <wheel>`; also handy locally.

    python scripts/smoke_test.py
"""

import faulthandler
import gc
import sys

import numpy as np

faulthandler.enable()


def main() -> int:
    import autoremesher
    from autoremesher import ModelType, Vector3, vertices_to_numpy

    print(f"python {sys.version.split()[0]} | autoremesher {autoremesher.__version__}", flush=True)

    # --- low-level API ------------------------------------------------------
    verts = [Vector3(x, y, z) for x in (0.0, 1.0) for y in (0.0, 1.0) for z in (0.0, 1.0)]
    tris = [
        [0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5],
        [0, 4, 5], [0, 5, 1], [2, 3, 7], [2, 7, 6],
        [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3],
    ]

    # Diagnostic: first try remesh() WITHOUT a progress callback to isolate
    # whether the crash is in the callback trampoline or in the core code.
    print("[diag] remesh without callback...", flush=True)
    r0 = autoremesher.AutoRemesher(verts, tris)
    r0.set_target_triangle_count(200)
    r0.set_model_type(ModelType.Organic)
    assert r0.remesh(), "remesh() without callback failed"
    print("[diag] remesh without callback OK", flush=True)
    del r0
    gc.collect()

    progress = []
    remesher = autoremesher.AutoRemesher(verts, tris)
    remesher.set_target_triangle_count(200)
    remesher.set_model_type(ModelType.Organic)
    remesher.set_progress_callback(lambda p, s: progress.append(p))
    print("[diag] remesh with callback...", flush=True)
    assert remesher.remesh(), "remesh() failed"

    out_vertices = remesher.get_remeshed_vertices()
    out_faces = remesher.get_remeshed_quads()
    assert len(out_vertices) > 0 and len(out_faces) > 0, "empty result"
    n_quads = sum(1 for f in out_faces if len(f) == 4)
    assert n_quads / len(out_faces) > 0.5, f"not quad-dominant: {n_quads}/{len(out_faces)}"
    assert progress and progress[0] == 0.0 and progress[-1] == 1.0, "progress not 0..1"
    assert all(a <= b + 1e-6 for a, b in zip(progress, progress[1:])), "progress not monotonic"
    assert any("Total:" in line for line in remesher.phase_report()), "no phase report"

    np_vertices = vertices_to_numpy(out_vertices)
    assert np_vertices.shape == (len(out_vertices), 3), np_vertices.shape

    remesher.set_progress_callback(None)
    del remesher, out_vertices, out_faces, verts, tris
    gc.collect()

    # --- high-level convenience API ------------------------------------------
    cube_v = np.array(
        [(x, y, z) for x in (0.0, 1.0) for y in (0.0, 1.0) for z in (0.0, 1.0)],
        dtype=np.float64)
    cube_t = np.array(tris := [
        [0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5],
        [0, 4, 5], [0, 5, 1], [2, 3, 7], [2, 7, 6],
        [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3],
    ], dtype=np.uint64)

    new_v, new_f = autoremesher.remesh(cube_v, cube_t, target_quads=200,
                                       adaptivity=1.0, anisotropy=1.0)
    assert new_v.shape[0] > 0 and len(new_f) > 0, "convenience remesh() empty"

    print(f"smoke test OK: {new_v.shape[0]} vertices, {len(new_f)} faces, "
          f"{sum(1 for f in new_f if len(f) == 4)} quads", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
