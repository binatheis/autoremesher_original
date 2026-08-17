#!/usr/bin/env python3
"""Assemble .whl files from the compiled nanobind extension modules.

A wheel is a zip archive with a specific layout:
    autoremesher/__init__.py
    autoremesher/autremesh.<ext>
    autoremesher-<version>.dist-info/METADATA
    autoremesher-<version>.dist-info/WHEEL
    autoremesher-<version>.dist-info/RECORD

This script is dependency-free (stdlib only) so it runs on any Python that
happens to be present in CI, regardless of the interpreter being packaged.

Usage:
    python scripts/package_wheels.py \
        --build-dir build/build/Release \
        --version 1.1.0 \
        --platform-tag win_amd64 \
        --outdir dist
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import os
import zipfile
from pathlib import Path

DIST_NAME = "autoremesher"

METADATA_TEMPLATE = """Metadata-Version: 2.1
Name: autoremesher
Version: {version}
Summary: Automatic quad remeshing: convert triangle meshes into quad-dominant meshes
Home-page: https://github.com/huxingyi/autoremesher
License: MIT
Requires-Python: {requires_python}
Description-Content-Type: text/markdown

AutoRemesher converts high-polygon triangle meshes into clean quad-based
topology using a native QuadCover parameterizer. Binary wheel with the
nanobind-based engine (see docs/PYTHON.md in the repository for the full API).
"""

WHEEL_TEMPLATE = """Wheel-Version: 1.0
Generator: package_wheels.py
Root-Is-Purelib: false
Tag: {tag}
"""


def b64digest(data: bytes) -> str:
    digest = hashlib.sha256(data).digest()
    return "sha256=" + base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")


def find_module(build_dir: Path, tag_dir: str) -> Path | None:
    folder = build_dir / "python" / tag_dir
    if not folder.is_dir():
        return None
    for entry in sorted(folder.iterdir()):
        if entry.suffix in (".pyd", ".so") and entry.name.startswith("autremesh"):
            return entry
    return None


def write_wheel(module_path: Path, init_py: Path, version: str,
                tag: str, requires_python: str, outdir: Path) -> Path:
    wheel_name = f"{DIST_NAME}-{version}-{tag}.whl"
    wheel_path = outdir / wheel_name
    dist_info = f"{DIST_NAME}-{version}.dist-info"

    records: list[tuple[str, str, str]] = []
    entries: list[tuple[str, bytes]] = []

    def add(arcname: str, data: bytes) -> None:
        entries.append((arcname, data))
        records.append((arcname, b64digest(data), str(len(data))))

    add(f"{DIST_NAME}/__init__.py", init_py.read_bytes())
    add(f"{DIST_NAME}/{module_path.name}", module_path.read_bytes())
    add(f"{dist_info}/METADATA",
        METADATA_TEMPLATE.format(version=version, requires_python=requires_python).encode())
    add(f"{dist_info}/WHEEL", WHEEL_TEMPLATE.format(tag=tag).encode())
    records.append((f"{dist_info}/RECORD", "", ""))

    with zipfile.ZipFile(wheel_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for arcname, data in entries:
            zf.writestr(arcname, data)
        record_text = "\n".join(f"{n},{d},{s}" for n, d, s in records) + "\n"
        zf.writestr(f"{dist_info}/RECORD", record_text)

    return wheel_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path,
                        help="CMake build directory containing python/<tag>/ modules")
    parser.add_argument("--version", required=True)
    parser.add_argument("--platform-tag", required=True,
                        help="e.g. win_amd64, macosx_12_0_arm64, manylinux_2_28_x86_64")
    parser.add_argument("--outdir", default=Path("dist"), type=Path)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    init_py = repo_root / "bindings" / "python" / DIST_NAME / "__init__.py"
    if not init_py.is_file():
        parser.error(f"package __init__.py not found at {init_py}")

    args.outdir.mkdir(parents=True, exist_ok=True)

    produced = []

    module_cp311 = find_module(args.build_dir, "cp311")
    if module_cp311:
        produced.append(write_wheel(
            module_cp311, init_py, args.version,
            f"cp311-cp311-{args.platform_tag}", ">=3.11", args.outdir))

    module_abi3 = find_module(args.build_dir, "abi3")
    if module_abi3:
        produced.append(write_wheel(
            module_abi3, init_py, args.version,
            f"cp312-abi3-{args.platform_tag}", ">=3.12", args.outdir))

    if not produced:
        parser.error(f"no extension modules found under {args.build_dir}/python/")

    for path in produced:
        print(f"built {path.name} ({path.stat().st_size / 1024:.0f} KiB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
