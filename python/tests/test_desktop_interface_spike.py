import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / "python"
if str(LAB) not in sys.path:
    sys.path.insert(0, str(LAB))


class DesktopInterfaceSpikeTests(unittest.TestCase):
    def test_windows_matrix_covers_spec_scenarios_and_boundary(self) -> None:
        report = json.loads(
            (ROOT / "validation/reports/desktop_interface_spike_windows.json").read_text(
                encoding="utf-8"
            )
        )
        expected = {
            "sdl2_initialization",
            "dear_imgui_context",
            "focus_non_stealing",
            "clipboard",
            "ime_pt_br",
            "keyboard",
            "dpi_100_250",
            "multiple_monitors",
            "transparency",
            "input_grab_disabled",
            "fullscreen",
            "screen_reader",
            "click_through",
            "tray",
            "suspend_resume",
            "idle_event_loop",
        }
        scenarios = {item["id"]: item for item in report["scenarios"]}
        self.assertEqual(set(scenarios), expected)
        self.assertEqual(report["schema_version"], "1.0")
        self.assertEqual(
            report["renderer_boundary"],
            "optional_spike_does_not_alter_avatar_presentation_port",
        )
        self.assertEqual(report["product_decision"], "sdl2_imgui_not_selected_for_product_shell")
        self.assertEqual(scenarios["focus_non_stealing"]["status"], "validated")
        self.assertEqual(scenarios["clipboard"]["status"], "validated")
        self.assertNotEqual(scenarios["fullscreen"]["status"], "validated")
        self.assertEqual(scenarios["screen_reader"]["status"], "unsupported")
        self.assertEqual(scenarios["tray"]["status"], "unsupported")

    def test_baseline_and_ablations_are_explicit(self) -> None:
        baseline = json.loads(
            (ROOT / "validation/ablations/desktop_interface_spike_baseline.json").read_text(
                encoding="utf-8"
            )
        )
        no_imgui = json.loads(
            (ROOT / "validation/ablations/desktop_interface_spike_no_imgui.json").read_text(
                encoding="utf-8"
            )
        )
        no_transparency = json.loads(
            (ROOT / "validation/ablations/desktop_interface_spike_no_transparency.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(baseline["mode"], "baseline_sdl2_without_imgui")
        self.assertEqual(no_imgui["mode"], "ablation_sdl2_without_imgui")
        self.assertEqual(no_transparency["mode"], "ablation_without_transparency")
        self.assertEqual(
            next(item for item in baseline["scenarios"] if item["id"] == "dear_imgui_context")["status"],
            "ablation",
        )
        self.assertEqual(
            next(item for item in no_transparency["scenarios"] if item["id"] == "transparency")["status"],
            "ablation",
        )


if __name__ == "__main__":
    unittest.main()
