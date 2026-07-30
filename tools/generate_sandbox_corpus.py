"""Generate the versioned synthetic corpus used by SPEC-017."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.sandbox import GENERATOR_VERSION, generate_session, split_sessions, write_session  # noqa: E402


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def generate_corpus(output: Path, session_count: int, seed: int, split_seed: int) -> dict[str, object]:
    if session_count < 3:
        raise ValueError("session_count must be at least 3")
    sessions = [generate_session(seed=seed + index, routine_count=4) for index in range(session_count)]
    splits = split_sessions(sessions, seed=split_seed)
    output.mkdir(parents=True, exist_ok=True)
    manifest_splits: dict[str, list[dict[str, str]]] = {}
    for split_name, split_sessions_value in splits.items():
        split_directory = output / split_name
        split_directory.mkdir(parents=True, exist_ok=True)
        records: list[dict[str, str]] = []
        for session in split_sessions_value:
            path = split_directory / f"session-{session.session_id}.json"
            write_session(session, str(path))
            records.append(
                {
                    "path": path.relative_to(output).as_posix(),
                    "session_id": session.session_id,
                    "sha256": sha256(path),
                }
            )
        manifest_splits[split_name] = records

    manifest = {
        "schema_version": "1.0",
        "corpus_id": "sandbox-v1",
        "generator_version": GENERATOR_VERSION,
        "seed": seed,
        "split_seed": split_seed,
        "session_count": session_count,
        "llm_dependency": False,
        "holdout_split": "test",
        "splits": manifest_splits,
    }
    manifest_path = output / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=REPOSITORY_ROOT / "datasets" / "synthetic" / "v1")
    parser.add_argument("--sessions", type=int, default=6)
    parser.add_argument("--seed", type=int, default=20260728)
    parser.add_argument("--split-seed", type=int, default=1701)
    args = parser.parse_args()
    manifest = generate_corpus(args.output, args.sessions, args.seed, args.split_seed)
    print(f"generated {manifest['session_count']} sessions in {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
