"""Reference validation for consented screen capture and local OCR policy."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .schema_validation import SchemaValidationError, validate_shared_schema


class ScreenOcrPolicyError(ValueError):
    """Raised when a screen/OCR capture policy is unsafe or inconsistent."""


def _validate_schema(value: dict[str, Any]) -> None:
    try:
        validate_shared_schema(value, "screen_ocr_capture_policy.schema.json")
    except (SchemaValidationError, OSError, json.JSONDecodeError) as error:
        raise ScreenOcrPolicyError(str(error)) from error


@dataclass(frozen=True)
class ScreenOcrPolicy:
    data: dict[str, Any]

    @classmethod
    def from_mapping(cls, value: dict[str, Any]) -> ScreenOcrPolicy:
        if not isinstance(value, dict):
            raise ScreenOcrPolicyError("screen OCR policy must be an object")
        _validate_schema(value)
        mode = value["capture_mode"]
        consent = value["consent_state"]
        request_id = value["request_id"]
        if mode == "explicit" and (consent != "granted" or request_id is not None):
            raise ScreenOcrPolicyError("explicit capture requires granted consent and no request id")
        if mode == "on_demand" and (consent != "granted" or not isinstance(request_id, str) or not request_id):
            raise ScreenOcrPolicyError("on-demand capture requires granted consent and a request id")
        if mode == "disabled" and request_id is not None:
            raise ScreenOcrPolicyError("disabled capture cannot carry a request id")
        region = value["region_of_interest"]
        if region is not None and (
            not isinstance(region, dict)
            or not all(isinstance(region[key], int) and not isinstance(region[key], bool)
                       for key in ("x", "y", "width", "height"))
            or region["x"] < 0
            or region["y"] < 0
            or region["width"] <= 0
            or region["height"] <= 0
        ):
            raise ScreenOcrPolicyError("region of interest must be positive")
        if value["text_retention_days"] >= value["visual_retention_days"]:
            raise ScreenOcrPolicyError("text retention must be shorter than visual retention")
        if value["capture_mode"] == "disabled" and value["consent_state"] == "granted":
            # A granted but disabled policy is valid: it represents a paused UI choice.
            pass
        return cls(json.loads(json.dumps(value, ensure_ascii=False)))

    @classmethod
    def load(cls, path: str | Path) -> ScreenOcrPolicy:
        try:
            value = json.loads(Path(path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ScreenOcrPolicyError(str(error)) from error
        return cls.from_mapping(value)

    def allows_capture(self, request_id: str | None = None) -> bool:
        if self.data["global_pause"] or self.data["consent_state"] != "granted":
            return False
        if self.data["capture_mode"] == "explicit":
            return True
        return (
            self.data["capture_mode"] == "on_demand"
            and bool(request_id)
            and request_id == self.data["request_id"]
        )

    @staticmethod
    def redact_text(value: str) -> str:
        return f"[redacted:length={len(value)}]"


def validate_screen_ocr_fixtures(root: str | Path) -> None:
    repository = Path(root)
    ScreenOcrPolicy.load(repository / "contracts/fixtures/screen_ocr_capture_policy.json")
    ScreenOcrPolicy.load(repository / "contracts/fixtures/screen_ocr_capture_policy.default.json")
    try:
        ScreenOcrPolicy.load(repository / "contracts/fixtures/screen_ocr_capture_policy.invalid.json")
    except ScreenOcrPolicyError:
        return
    raise ScreenOcrPolicyError("invalid screen OCR fixture was accepted")
