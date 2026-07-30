import sys
import unittest
from pathlib import Path

LAB_ROOT = Path(__file__).resolve().parents[1]
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.schema_validation import (
    SchemaValidationError,
    validate_shared_schema,
)


def profile() -> dict[str, object]:
    return {
        "profile_id": "profile-1",
        "schema_version": "1.0",
        "source": "procedural",
        "motif": "neutral_field",
        "profile_version": "1.0",
        "shape": "filament",
        "density": 0.5,
        "turbulence": 0.2,
        "glow": 0.35,
        "palette": ["#18485C", "#5ABEAe"],
        "speed": 0.2,
        "cohesion": 0.65,
        "override": {
            "active": False,
            "override_id": None,
            "reason": None,
            "version": "1.0",
        },
    }


class AvatarPresentationProfileTests(unittest.TestCase):
    def test_profile_schema_accepts_bounded_procedural_profile(self) -> None:
        validate_shared_schema(profile(), "avatar_presentation_profile.schema.json")

    def test_profile_schema_rejects_non_procedural_and_invalid_palette(self) -> None:
        invalid_source = profile()
        invalid_source["source"] = "visual-model"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid_source, "avatar_presentation_profile.schema.json")

        invalid_palette = profile()
        invalid_palette["palette"] = ["#18485C", "not-a-color"]
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid_palette, "avatar_presentation_profile.schema.json")

    def test_profile_schema_rejects_unbounded_values_and_extra_fields(self) -> None:
        invalid_value = profile()
        invalid_value["density"] = 1.01
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid_value, "avatar_presentation_profile.schema.json")

        extra = profile()
        extra["emotion"] = "happy"
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(extra, "avatar_presentation_profile.schema.json")


if __name__ == "__main__":
    unittest.main()
