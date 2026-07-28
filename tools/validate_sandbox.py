"""Validate the SPEC-017 corpus manifest, hashes and ground truth references."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_corpus(corpus: Path) -> tuple[int, int]:
    manifest_path = corpus / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != "1.0":
        raise ValueError("unsupported corpus schema")
    if manifest.get("llm_dependency") is not False:
        raise ValueError("corpus must declare llm_dependency=false")
    if manifest.get("holdout_split") != "test":
        raise ValueError("test split must remain the locked holdout")

    seen_sessions: set[str] = set()
    file_count = 0
    for split_name in ("train", "development", "test"):
        records = manifest.get("splits", {}).get(split_name)
        if not isinstance(records, list) or not records:
            raise ValueError(f"split {split_name} must be a non-empty list")
        for record in records:
            session_id = record["session_id"]
            if session_id in seen_sessions:
                raise ValueError(f"session appears in multiple splits: {session_id}")
            seen_sessions.add(session_id)
            path = corpus / record["path"]
            if not path.is_file():
                raise ValueError(f"missing session file: {path}")
            if sha256(path) != record["sha256"]:
                raise ValueError(f"hash mismatch: {path}")
            session = json.loads(path.read_text(encoding="utf-8"))
            event_ids = {event["event_id"] for event in session["events"]}
            episode_ids = {episode["episode_id"] for episode in session["episodes"]}
            if session["session_id"] != session_id:
                raise ValueError(f"session id mismatch: {path}")
            if any(event_id not in event_ids for episode in session["episodes"] for event_id in episode["event_ids"]):
                raise ValueError(f"episode references unknown event: {path}")
            if any(link["event_id"] not in event_ids for link in session["causal_links"]):
                raise ValueError(f"causal link references unknown event: {path}")
            if set(session["episode_ids"]) != episode_ids:
                raise ValueError(f"episode_ids index mismatch: {path}")
            file_count += 1

    if file_count != manifest["session_count"]:
        raise ValueError("manifest session_count does not match split files")
    return file_count, len(seen_sessions)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    args = parser.parse_args()
    files, sessions = validate_corpus(args.corpus)
    print(f"valid sandbox corpus: {sessions} sessions, {files} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
