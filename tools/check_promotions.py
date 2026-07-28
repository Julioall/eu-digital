"""Fail CI when a required component has no approved promotion record."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.promotion import (
    PromotionGateError,
    PromotionRegistry,
    check_required_components,
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--required",
        type=Path,
        default=ROOT / "promotions" / "required_components.json",
    )
    parser.add_argument(
        "--registry", type=Path, default=ROOT / "promotions" / "registry.json"
    )
    parser.add_argument("--component", action="append", dest="components")
    args = parser.parse_args()

    if args.components:
        registry = PromotionRegistry(args.registry)
        missing = []
        for component in args.components:
            try:
                registry.require(component)
            except PromotionGateError:
                missing.append(component)
    else:
        missing = check_required_components(args.required, args.registry)
    if missing:
        print("missing approved promotions: " + ", ".join(missing), file=sys.stderr)
        return 1
    print("promotion gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
