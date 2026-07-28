"""Validate the shared CanonicalEvent schema and fixture."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.fixture_reader import read_canonical_event  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=REPOSITORY_ROOT / "contracts" / "fixtures" / "canonical_event.json",
    )
    args = parser.parse_args()
    event = read_canonical_event(args.fixture, REPOSITORY_ROOT / "contracts" / "schemas" / "canonical_event.schema.json")
    print(f"valid CanonicalEvent: {event['event_id']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
