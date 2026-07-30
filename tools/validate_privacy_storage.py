"""Validate local consent, storage, health, and data-management contracts."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))

from eu_digital_lab.privacy_storage import validate_privacy_storage_fixtures  # noqa: E402


if __name__ == "__main__":
    validate_privacy_storage_fixtures(ROOT)
    print("valid privacy and storage contracts")
