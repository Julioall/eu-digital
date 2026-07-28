"""Run the SPEC-025 Python/C++ validation flow."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


def run(command: list[str], root: Path) -> None:
    print("$", " ".join(command))
    subprocess.run(command, cwd=root, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if shutil.which("cmake") is None and not args.skip_build:
        raise SystemExit("cmake is required; install CMake before running the hybrid validation")

    run(["python", "-m", "unittest", "discover", "-s", "python/tests", "-v"], root)
    if args.skip_build:
        return 0
    run(["cmake", "--preset", "dev"], root)
    run(["cmake", "--build", "--preset", "dev"], root)
    run(["ctest", "--test-dir", "build/dev", "--output-on-failure"], root)

    release = root / "build" / "release"
    run(["cmake", "--install", "build/dev", "--prefix", str(release)], root)
    installed_python = list(release.rglob("*.py")) if release.exists() else []
    if installed_python:
        raise SystemExit(f"release contains Python files: {installed_python}")
    print("hybrid validation passed; release contains no Python runtime files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
