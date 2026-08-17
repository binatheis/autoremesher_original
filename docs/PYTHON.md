# AutoRemesher Python — Referencia Completa del Wheel

Bindings de Python para el motor AutoRemesher v1.1.0, construidos con
**nanobind** (módulo nativo `autremesh`) y empaquetados como wheels
binarios multi-plataforma.

## Compatibilidad

| Wheel | Tag | Sirve para |
|-------|-----|-----------|
| `autoremesher-<ver>-cp311-cp311-<plat>.whl` | cp311 | Python 3.11 |
| `autoremesher-<ver>-cp312-abi3-<plat>.whl` | cp312-abi3 | Python **3.12, 3.13 y 3.14** (Stable ABI) |

Plataformas (`<plat>`): `win_amd64`, `macosx_12_0_arm64`, `manylinux_2_28_x86_64`.

Los wheels son autocontenidos: TBB, Eigen, meshoptimizer e isotropicremesher
van enlazados estáticamente. No requieren redistribuibles de terceros más allá
del runtime estándar de CPython.

## Instalación

```bash
# Descarga el wheel de tu plataforma/versión desde los artefactos de CI, luego:
pip install autoremesher-1.1.0-cp312-abi3-win_amd64.whl   # ejemplo Windows, py>=3.12
pip install autoremesher-1.1.0-cp311-cp311-win_amd64.whl  # ejemplo Windows, py3.11
```

NumPy solo es necesario si usas los helpers de conversión o la API de
conveniencia (`pip install numpy`).

---

## API de alto nivel (recomendada)

### `autoremesher.remesh(...) -> (vertices, faces)`

```python
import numpy as np
import autoremesher

vertices = np.array([...], dtype=np.float64)   # (N, 3)
triangles = np.array([...], dtype=np.uint64)   # (M, 3), índices 0-based

new_vertices, new_faces = autoremesher.remesh(
    vertices, triangles,
    target_quads=50000,
    edge_scaling=1.0,
    sharp_edge=90.0,
    smooth_normal=0.0,
    adaptivity=1.0,
    anisotropy=1.0,
    on_progress=lambda p, s: print(f"{p*100:.1f}%  {s}"),
)
```

**Parámetros** (idéntica semántica al CLI, ver `docs/CLI.md`):

| Parámetro | Tipo | Default | Descripción |
|-----------|------|---------|-------------|
| `vertices` | `(N,3)` float64 ndarray o `list[Vector3]` | requerido | Vértices de entrada |
| `triangles` | `(M,3)` uint64 ndarray o `list[list[int]]` | requerido | Caras triangulares de entrada |
| `target_quads` | int | `50000` | Quads objetivo en la salida (internamente ×2 triángulos) |
| `edge_scaling` | float | `1.0` | Escala de aristas `[1.0, 4.0]`; mayor = más low-poly |
| `sharp_edge` | float | `90.0` | Umbral diedro (grados) para aristas sharp |
| `smooth_normal` | float | `0.0` | Suavizado de normales (grados) para low-poly; `0.0` off |
| `adaptivity` | float | `1.0` | Densidad adaptativa por curvatura `[0.0, 1.0]` |
| `anisotropy` | float | `1.0` | Elongación de quads por curvatura `[0.0, 1.0]` |
| `on_progress` | callable | `None` | `fn(progress: float, status: str)`, llamado desde threads worker |

**Retorna:** `(vertices, faces)` donde `vertices` es `(K,3)` float64 ndarray y
`faces` es `list[list[int]]` (quads de 4 índices más algunos triángulos).

**Errores:** lanza `RuntimeError` si el remeshing falla.

---

## API de bajo nivel (control total)

### `class autoremesher.AutoRemesher`

```python
from autoremesher import AutoRemesher, ModelType

r = AutoRemesher(vertices, triangles)   # listas de Vector3 / list[list[int]]
r.set_target_triangle_count(100000)     # int: triángulos objetivo (quads × 2)
r.set_scaling(1.0)                      # float: edge scaling
r.set_model_type(ModelType.Organic)     # ModelType.Organic | ModelType.HardSurface
r.set_gradient_adaptivity(1.0)          # float: [0.0, 1.0]
r.set_sharp_edge_degrees(90.0)          # float: grados
r.set_smooth_normal_degrees(0.0)        # float: grados
r.set_anisotropy(1.0)                   # float: [0.0, 1.0]

r.set_progress_callback(lambda progress, status: ...)   # None para quitar

ok = r.remesh()                         # bool; libera el GIL durante el cómputo

verts = r.get_remeshed_vertices()       # list[Vector3]
faces = r.get_remeshed_quads()          # list[list[int]]
report = r.phase_report()               # list[str]: timings por fase del último remesh
```

| Método | Firma | Descripción |
|--------|-------|-------------|
| `set_target_triangle_count` | `(int)` | Triángulos objetivo del remeshing uniforme (el GUI usa `quads × 2`) |
| `set_scaling` | `(float)` | Factor de escala de aristas |
| `set_model_type` | `(ModelType)` | `Organic` (default) o `HardSurface` |
| `set_gradient_adaptivity` | `(float)` | Densidad adaptativa por curvatura |
| `set_sharp_edge_degrees` | `(float)` | Umbral diedro para aristas sharp |
| `set_anisotropy` | `(float)` | Elongación anisotrópica de quads |
| `set_smooth_normal_degrees` | `(float)` | Suavizado de normales |
| `set_progress_callback` | `(callable \| None)` | `fn(float progress, str status)`; invocado desde threads worker (el callback adquiere el GIL automáticamente) |
| `remesh` | `() -> bool` | Ejecuta el pipeline completo; **libera el GIL** mientras computa |
| `get_remeshed_vertices` | `() -> list[Vector3]` | Vértices del resultado |
| `get_remeshed_quads` | `() -> list[list[int]]` | Caras del resultado (mayormente quads) |
| `phase_report` | `() -> list[str]` | Tiempos por fase (ej. `"Parameterize (accumulated): 68.5 ms"`, `"Total: 103.7 ms"`) |

### `class autoremesher.Vector3`

```python
v = Vector3(1.0, 2.0, 3.0)
v.x, v.y, v.z            # propiedades de lectura/escritura
v[0], v[1], v[2]         # indexación
v.length()               # float
v.length_squared()       # float
v.normalized()           # Vector3
v.normalize()            # in-place
v.is_zero()              # bool

v + v2, v - v2, -v       # aritmética
v * 2.0, 2.0 * v, v / 2.0

Vector3.dot_product(a, b)      # float
Vector3.cross_product(a, b)    # Vector3
Vector3.normal(a, b, c)        # normal del triángulo
Vector3.area(a, b, c)          # área del triángulo
Vector3.angle(a, b)            # ángulo en radianes
```

### `class autoremesher.ModelType`

Enum: `ModelType.Organic` (default, para mallas orgánicas) y
`ModelType.HardSurface` (para modelos CAD/mecánicos).

### Helpers NumPy

```python
from autoremesher import vertices_from_numpy, vertices_to_numpy, faces_from_numpy

verts = vertices_from_numpy(np_array_nx3_float64)   # -> list[Vector3]
arr = vertices_to_numpy(list_de_vector3)            # -> (N,3) float64 ndarray
faces = faces_from_numpy(np_array_nx3_o_nx4_uint64) # -> list[list[int]]
```

> Nota: la API de alto nivel `autoremesher.remesh()` ya hace estas
> conversiones automáticamente si le pasas ndarrays.

### `autoremesher.__version__`

Versión del motor (p. ej. `"1.1.0"`).

---

## Concurrencia y GIL

- `remesh()` libera el GIL: los threads TBB del motor corren en paralelo sin
  bloquear el intérprete.
- El callback de progreso se invoca desde threads worker; nanobind adquiere el
  GIL antes de llamar a tu función Python. Mantén el callback ligero.
- Instancias distintas de `AutoRemesher` pueden usarse desde distintos threads
  Python (cada una tiene su propio callback).

## Limpieza de recursos

Los objetos viven bajo el GC de Python. Si tu proceso imprime warnings de
nanobind sobre leaks al salir, es porque quedaron instancias vivas al final del
intérprete; basta con hacer `del` de los objetos y `gc.collect()` (o dejar que
el proceso termine — es cosmético y no afecta al exit code).

## Ejemplo end-to-end

```python
import numpy as np
import autoremesher

# Cargar una malla (aquí con cualquier librería; p. ej. trimesh):
import trimesh
mesh = trimesh.load("armadillo.obj")
v = np.asarray(mesh.vertices, dtype=np.float64)
t = np.asarray(mesh.faces, dtype=np.uint64)

progress_last = -1
def on_progress(p, s):
    global progress_last
    if int(p * 100) != progress_last:
        progress_last = int(p * 100)
        print(f"\r{progress_last}% {s}", end="", flush=True)

out_v, out_f = autoremesher.remesh(v, t, target_quads=20000,
                                   adaptivity=1.0, anisotropy=1.0,
                                   on_progress=on_progress)

result = trimesh.Trimesh(vertices=out_v,
                         faces=[f for f in out_f if len(f) == 3] +
                               [f for f in out_f if len(f) == 4],
                         process=False)
result.export("armadillo_quad.obj")
```
