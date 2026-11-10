#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run real C implementations against an in-memory KGSL syscall fake."""
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / "build/host-tests"
out.mkdir(parents=True, exist_ok=True)
cc = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
if not cc:
    raise SystemExit("A host C compiler is required")
binary = out / ("adreno-test.exe" if os.name == "nt" else "adreno-test")
sources = ["tests/adreno_test.c"] + [f"lib/gpu/adreno/{name}.c" for name in ("kgsl", "raw", "a7xx", "a8xx")]
subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
                "-Itests/platform", "-Iinclude", "-Ithird_party", *sources,
                "-o", str(binary)], cwd=root, check=True)
subprocess.run([str(binary)], check=True)
