"""Validate ScreenOcrCapturePolicy fixtures."""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

from eu_digital_lab.screen_ocr_policy import validate_screen_ocr_fixtures

if __name__ == "__main__":
    validate_screen_ocr_fixtures(ROOT)
    print("valid consented screen OCR policies")
