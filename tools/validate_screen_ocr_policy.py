"""Validate ScreenOcrCapturePolicy fixtures."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

from eu_digital_lab.screen_ocr_policy import validate_screen_ocr_fixtures  # noqa: E402


if __name__ == "__main__":
    validate_screen_ocr_fixtures(ROOT)
    print("valid consented screen OCR policies")
