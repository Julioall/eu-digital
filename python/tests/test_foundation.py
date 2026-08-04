import json
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LAB_ROOT = REPOSITORY_ROOT / "python"
if str(LAB_ROOT) not in sys.path:
    sys.path.insert(0, str(LAB_ROOT))

from eu_digital_lab.fixture_reader import read_canonical_event


class FoundationTests(unittest.TestCase):
    def test_python_package_metadata_is_isolated(self) -> None:
        metadata = tomllib.loads((REPOSITORY_ROOT / "pyproject.toml").read_text(encoding="utf-8"))
        project = metadata["project"]
        self.assertEqual(project["name"], "eu-digital-lab")
        self.assertEqual(project["requires-python"], ">=3.12")
        self.assertEqual(project.get("dependencies", []), [])

    def test_python_and_fixture_share_canonical_event(self) -> None:
        fixture_path = REPOSITORY_ROOT / "contracts" / "fixtures" / "canonical_event.json"
        event = read_canonical_event(fixture_path)
        raw = json.loads(fixture_path.read_text(encoding="utf-8"))
        self.assertEqual(event["event_id"], raw["event_id"])
        self.assertEqual(event["event_type"], "fixture.canonical_event")
        self.assertEqual(event["schema_version"], "1.0")

    def test_incompatible_contract_fixture_is_rejected(self) -> None:
        fixture_path = REPOSITORY_ROOT / "contracts" / "fixtures" / "canonical_event.json"
        value = json.loads(fixture_path.read_text(encoding="utf-8"))
        value.pop("event_type")
        with tempfile.TemporaryDirectory() as directory:
            invalid_fixture = Path(directory) / "invalid.json"
            invalid_fixture.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaises(ValueError):
                read_canonical_event(
                    invalid_fixture,
                    REPOSITORY_ROOT / "contracts" / "schemas" / "canonical_event.schema.json",
                )

    def test_cmake_has_no_python_runtime_dependency(self) -> None:
        cmake = (REPOSITORY_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("Python_EXECUTABLE", cmake)
        self.assertNotIn("python_runtime", cmake.lower())
        presets = (REPOSITORY_ROOT / "CMakePresets.json").read_text(encoding="utf-8")
        self.assertIn('"name": "dev"', presets)
        self.assertIn('"generator": "Ninja"', presets)


if __name__ == "__main__":
    unittest.main()
