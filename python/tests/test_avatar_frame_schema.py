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


def frame() -> dict[str, object]:
    return {
        "schema_version": "1.0",
        "source": "procedural",
        "model_required": False,
        "width": 64,
        "height": 64,
        "rendered": True,
        "reason": "rendered",
        "pixel_count": 4096,
        "nonzero_pixel_count": 721,
        "pixels_sha256": "0" * 64,
        "blocks_work": False,
        "captures_input": False,
        "has_focus": False,
    }


class AvatarFrameSchemaTests(unittest.TestCase):
    def test_rendered_frame_contract_is_valid(self) -> None:
        validate_shared_schema(frame(), "avatar_frame.schema.json")

    def test_frame_contract_rejects_model_or_focus(self) -> None:
        invalid_model = frame()
        invalid_model["model_required"] = True
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid_model, "avatar_frame.schema.json")

        invalid_focus = frame()
        invalid_focus["has_focus"] = True
        with self.assertRaises(SchemaValidationError):
            validate_shared_schema(invalid_focus, "avatar_frame.schema.json")


if __name__ == "__main__":
    unittest.main()
