"""Validate the separate component maturity registry."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.component_maturity import ComponentMaturityRegistry


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry",
        type=Path,
        default=ROOT / "contracts" / "fixtures" / "component_maturity.json",
    )
    parser.add_argument("--specs", type=Path, default=ROOT / "specs")
    args = parser.parse_args()
    registry = ComponentMaturityRegistry.load(args.registry)
    registry.validate_spec_references(args.specs)
    registry.validate_evidence_references(ROOT)
    print(f"valid component maturity registry: {len(registry.data['components'])} components")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
