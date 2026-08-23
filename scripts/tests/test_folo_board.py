import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BOARD_DIR = ROOT / "main/boards/folo/ai-passport-c3"


class FoloBoardRepositoryTests(unittest.TestCase):
    def test_only_folo_board_is_shipped(self):
        boards_root = ROOT / "main/boards"
        self.assertEqual(
            sorted(path.name for path in boards_root.iterdir() if path.is_dir()),
            ["common", "folo"],
        )
        self.assertEqual(
            sorted(path.name for path in (boards_root / "folo").iterdir() if path.is_dir()),
            ["ai-passport-c3"],
        )

    def test_common_hardware_is_badge_scoped(self):
        common = ROOT / "main/boards/common"
        self.assertEqual(
            sorted(path.name for path in common.iterdir() if path.is_file()),
            [
                "backlight.cc",
                "backlight.h",
                "blufi.cpp",
                "blufi.h",
                "board.cc",
                "board.h",
                "button.cc",
                "button.h",
                "camera.h",
                "wifi_board.cc",
                "wifi_board.h",
            ],
        )

    def test_board_identity_and_target(self):
        config = json.loads((BOARD_DIR / "config.json").read_text(encoding="utf-8"))
        self.assertEqual(config["manufacturer"], "folo")
        self.assertEqual(config["type"], "folo-ai-passport-c3")
        self.assertEqual(config["target"], "esp32c3")
        self.assertEqual([build["name"] for build in config["builds"]], ["folo-ai-passport-c3"])

    def test_selection_chain_contains_one_board(self):
        kconfig = (ROOT / "main/Kconfig.projbuild").read_text(encoding="utf-8")
        cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
        symbols = re.findall(r"^\s*config (BOARD_TYPE_[A-Za-z0-9_]+)", kconfig, re.MULTILINE)
        self.assertEqual(symbols, ["BOARD_TYPE_FOLO_AI_PASSPORT_C3"])
        self.assertIn("CONFIG_BOARD_TYPE_FOLO_AI_PASSPORT_C3", cmake)
        self.assertIn('set(BOARD_DIR "folo/ai-passport-c3")', cmake)
        self.assertNotIn("elseif(CONFIG_BOARD_TYPE_", cmake)

    def test_exactly_one_board_factory(self):
        declarations = []
        for source in (ROOT / "main/boards").rglob("*.cc"):
            text = source.read_text(encoding="utf-8", errors="replace")
            declarations.extend(re.findall(r"\bDECLARE_BOARD\s*\(", text))
        self.assertEqual(len(declarations), 1)

    def test_badge_pin_contract(self):
        config = (BOARD_DIR / "config.h").read_text(encoding="utf-8")
        expected = {
            "AUDIO_I2S_GPIO_MCLK": "GPIO_NUM_6",
            "AUDIO_I2S_GPIO_BCLK": "GPIO_NUM_5",
            "AUDIO_I2S_GPIO_WS": "GPIO_NUM_3",
            "AUDIO_I2S_GPIO_DOUT": "GPIO_NUM_2",
            "AUDIO_I2S_GPIO_DIN": "GPIO_NUM_4",
            "DISPLAY_SPI_MOSI_PIN": "GPIO_NUM_9",
            "DISPLAY_SPI_SCK_PIN": "GPIO_NUM_8",
            "DISPLAY_SPI_CS_PIN": "GPIO_NUM_1",
            "DISPLAY_DC_PIN": "GPIO_NUM_20",
            "DISPLAY_BACKLIGHT_PIN": "GPIO_NUM_21",
            "BUTTON_ADC_CHANNEL": "ADC_CHANNEL_0",
        }
        for macro, value in expected.items():
            self.assertRegex(config, rf"#define\s+{macro}\s+{value}\b")


if __name__ == "__main__":
    unittest.main()
