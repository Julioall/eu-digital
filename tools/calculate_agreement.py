"""Calculate inter-annotator agreement for two annotation JSON files."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.annotation import calculate_agreement


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=Path)
    parser.add_argument("second", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    first = json.loads(args.first.read_text(encoding="utf-8"))
    second = json.loads(args.second.read_text(encoding="utf-8"))
    result = calculate_agreement(first, second)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote agreement to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
