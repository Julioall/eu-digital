from __future__ import annotations

import json
import unittest
from pathlib import Path

from eu_digital_lab.schema_validation import (
    SchemaValidationError,
    validate_shared_schema,
)

ROOT = Path(__file__).resolve().parents[2]


class DesktopRuntimeContractTests(unittest.TestCase):
    def test_session_state_accepts_versioned_content_free_state(self) -> None:
        validate_shared_schema(
            {
                "schema_version": "1.0",
                "session_id": "desktop-session-1",
                "state": "degraded",
                "observed_at": "2026-08-04T12:00:00Z",
                "consent_ready": False,
                "active_sensor_ids": ["system_activity"],
                "paused": False,
                "previous_shutdown_unclean": True,
                "model_available": False,
                "reason_code": "previous_shutdown_unclean",
            },
            "desktop_session_state.schema.json",
        )

    def test_session_state_rejects_unknown_state_and_extra_content(self) -> None:
        value = {
            "schema_version": "1.0",
            "session_id": "desktop-session-1",
            "state": "running",
            "observed_at": "2026-08-04T12:00:00Z",
            "consent_ready": True,
            "active_sensor_ids": [],
            "paused": False,
            "previous_shutdown_unclean": False,
            "model_available": False,
            "reason_code": None,
        }
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(
                {**value, "state": "thinking"},
                "desktop_session_state.schema.json",
            )
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(
                {**value, "window_title": "must-not-cross-contract"},
                "desktop_session_state.schema.json",
            )

    def test_performance_sample_accepts_metrics_and_rejects_bad_unit(self) -> None:
        sample = {
            "schema_version": "1.0",
            "sample_id": "idle-mean",
            "observed_at": "2026-08-04T12:00:00Z",
            "metric": "idle_cpu",
            "statistic": "mean",
            "value": 0.4,
            "unit": "percent",
            "sample_count": 100,
            "environment": "Windows Qt 6 offscreen Debug",
        }
        validate_shared_schema(sample, "desktop_performance_sample.schema.json")
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(
                {**sample, "unit": "milliseconds"},
                "desktop_performance_sample.schema.json",
            )

    def test_schemas_are_strict_json_schema_2020_12(self) -> None:
        for name in (
            "desktop_session_state.schema.json",
            "desktop_performance_sample.schema.json",
        ):
            schema = json.loads(
                (ROOT / "contracts" / "schemas" / name).read_text(encoding="utf-8")
            )
            self.assertEqual(
                schema["$schema"], "https://json-schema.org/draft/2020-12/schema"
            )
            self.assertFalse(schema["additionalProperties"])


if __name__ == "__main__":
    unittest.main()
