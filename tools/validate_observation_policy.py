"""Validate the low-risk Windows observation policy contract."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.observation_policy import load_capture_policy

if __name__ == "__main__":
    load_capture_policy(ROOT / "contracts/fixtures/capture_policy.json")
    print("valid low-risk observation policy")
