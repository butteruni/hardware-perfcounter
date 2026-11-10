#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Import exact pinned blobs from a local Mesa checkout, then regenerate."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    **{name: "src/freedreno/registers/adreno/" + name for name in (
        "a7xx_perfcntrs.xml", "a8xx_perfcntrs.xml", "a7xx_perfcntrs.json", "a8xx_perfcntrs.json")},
    "freedreno_copyright.xml": "src/freedreno/registers/freedreno_copyright.xml",
    "freedreno_devices.py": "src/freedreno/common/freedreno_devices.py",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Local Mesa Git checkout")
    parser.add_argument("--revision", required=True, help="Explicit Mesa revision to pin")
    args = parser.parse_args()
    revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "--verify",
                                       args.revision + "^{commit}"], text=True).strip()
    blobs = {name: subprocess.check_output(["git", "-C", str(args.source), "show", revision + ":" + path])
             for name, path in FILES.items()}
    records = []
    for name, data in blobs.items():
        (ROOT / "third_party/adreno" / name).write_bytes(data)
        records.append(dict(file=name, url=f"https://gitlab.freedesktop.org/mesa/mesa/-/blob/{revision}/{FILES[name]}",
                            sha256=hashlib.sha256(data).hexdigest()))
    (ROOT / "third_party/adreno/sources.json").write_text(json.dumps(records, indent=2) + "\n", newline="\n")
    subprocess.run([sys.executable, str(ROOT / "scripts/generate_modern_adreno.py")], check=True)
    print("Pinned Mesa revision:", revision)


if __name__ == "__main__":
    main()
