# AutoRemesher CLI — Referencia Completa

`autoremesher-cli` es la interfaz de línea de comandos standalone (sin Qt) del motor
AutoRemesher v1.1.0. Convierte mallas triangulares en mallas de topología
quad-dominante usando el parameterizador QuadCover nativo del proyecto.

Binarios por plataforma (artefactos de GitHub Actions):

| Plataforma | Artefacto | Binario |
|-----------|-----------|---------|
| Linux x86_64 | `autoremesher-cli-linux-x86_64` | `autoremesher-cli` |
| macOS arm64 | `autoremesher-cli-macos-arm64` | `autoremesher-cli` |
| Windows x64 | `autoremesher-cli-windows-x64` | `autoremesher-cli.exe` |

Todos los binarios son autocontenidos (TBB y todo lo demás enlazado
estáticamente; en Windows solo se depende del runtime de MSVC, `/MD`).

---

## Uso

```
autoremesher-cli -i INPUT.obj -o OUTPUT.obj [opciones]
```

### Opciones

| Flag | Tipo | Default | Rango | Descripción |
|------|------|---------|-------|-------------|
| `-i`, `--input` | ruta | **requerido** | archivo .obj existente | Malla triangular de entrada. Caras con más de 3 vértices se triangulan automáticamente (fan). |
| `-o`, `--output` | ruta | **requerido** | — | Archivo .obj de salida. Contiene vértices (`v`) y caras (`f`, mayormente quads de 4 índices, algunos triángulos). Índices 1-based. |
| `--report` | ruta | *(opcional)* | — | Escribe un reporte de texto con parámetros, conteos (quads/non-quads/vértices), tiempo total y **timings por fase** del pipeline. |
| `--target-quads` | int | `50000` | > 0 | Número objetivo de quads en la salida. Internamente el motor apunta a `2 × target-quads` triángulos en la fase de remeshing uniforme. Valores mayores = malla más densa y detallada (y más lenta). |
| `--edge-scaling` | float | `1.0` | `[1.0, 4.0]` | Factor de escala de aristas. Valores > 1.0 producen quads más grandes (menos densos) respecto a `target-quads` — útil para low-poly. |
| `--sharp-edge` | float | `90.0` | `[30.0, 180.0]` | Umbral de ángulo diedro en grados. Aristas con ángulo mayor se consideran "sharp": el campo de quads se alinea a ellas y no se suavizan. Menor valor = más aristas tratadas como features. |
| `--smooth-normal` | float | `0.0` | `[0.0, 180.0]` | Umbral de ángulo diedro en grados por debajo del cual las normales se suavizan durante el remeshing isotrópico. Útil para entradas low-poly faceteadas. `0.0` desactiva. |
| `--adaptivity` | float | `1.0` | `[0.0, 1.0]` | Densidad adaptativa por curvatura: `0.0` = densidad uniforme, `1.0` = máxima concentración de quads en regiones de alta curvatura. |
| `--anisotropy` | float | `1.0` | `[0.0, 1.0]` | Elongación anisotrópica de quads (nuevo en v1.1.0): `0.0` = quads cuadrados (isotrópico), `1.0` = máximo estiramiento a lo largo de las direcciones de curvatura principal. |
| `-q`, `--quiet` | flag | off | — | Suprime el progreso porcentual y el resumen por stdout (los errores siguen yendo a stderr). |
| `-h`, `--help` | flag | — | — | Muestra la ayuda. |

### Códigos de salida

| Código | Significado |
|--------|-------------|
| `0` | Éxito |
| `1` | Error de parseo de argumentos (CLI11) |
| `2` | Error de E/S (no se pudo leer `--input` o escribir `--output`) |
| `3` | El proceso de remeshing falló |

### Progreso

En modo normal, el CLI imprime el progreso como porcentaje con el nombre de la
fase actual (actualización en línea con retorno de carro):

```
 37.4%  Eliminating cover constraints
```

---

## El pipeline que ejecuta

1. **Cálculo de voxel size** a partir del área total y `--target-quads`.
2. **Separación en islas** (`MeshSeparator`) — cada componente conexo se procesa en paralelo con TBB.
3. Por isla:
   - **Decimación** previa con meshoptimizer si la isla supera 8× el objetivo de triángulos.
   - **Remeshing isotrópico** uniforme (con campo de longitud adaptativo si `--adaptivity > 0`).
   - **Parameterización** QuadCover nativa: frame field (Eigen), simplificación de singularidades, campo de anisotropía, solve Mixed-Integer Least Squares.
   - **Extracción de quads** desde las isolíneas UV: trazado de conexiones, construcción del grafo, limpieza topológica (split de caras de 6/7 aristas, merge de caras de 5), suavizado y proyección sobre la malla original.
4. **Merge de islas** y escritura del OBJ.

---

## Ejemplos

### Remesh básico

```bash
autoremesher-cli -i armadillo.obj -o armadillo_quad.obj
```

### Low-poly (quads grandes)

```bash
autoremesher-cli -i model.obj -o model_lowpoly.obj \
  --target-quads 2000 --edge-scaling 2.0
```

### Hard surface (preservar aristas vivas)

```bash
autoremesher-cli -i mech.obj -o mech_quad.obj \
  --sharp-edge 60.0 --smooth-normal 30.0
```

### Máxima calidad orgánica con anisotropía

```bash
autoremesher-cli -i character.obj -o character_quad.obj \
  --target-quads 100000 --adaptivity 1.0 --anisotropy 1.0
```

### Batch/headless con reporte

```bash
autoremesher-cli -i in.obj -o out.obj --report report.txt --quiet
echo "exit code: $?"
```

El `report.txt` resultante incluye:

```
AutoRemesher Report
===================
Input file: in.obj
...
Results:
  Quads: 146
  Non-quads: 0
  Vertices: 148
  Total time: 0.104 seconds

Phase timings:
  Islands: 1, input triangles: 12
  Compute voxel size: 0.0 ms
  Split into islands: 0.1 ms
  Isotropic remesh (accumulated): 11.1 ms
  Parameterize (accumulated): 68.5 ms
  Quad extract (accumulated): 19.8 ms
  ...
  Total: 103.7 ms
```

---

## Notas

- La entrada se triangula si contiene quads/ngons, pero la **salida siempre es quad-dominante**.
- Mallas no-manifold son soportadas (corregido en v1.1.0), aunque la calidad puede degradarse.
- En mallas muy densas la decimación previa se activa automáticamente (8× el objetivo).
- El CLI usa todos los núcleos disponibles (TBB); no hay flag de límite de threads.
