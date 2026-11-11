# -*- coding: utf-8 -*-
"""SSLM Lighting Lab离线合同测试；不导入Unreal、不创建或修改UE资产。"""
import ast
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / "Tools/Unreal/World/SSLM_LightingLab_V1.json"
AUTHOR = ROOT / "Tools/Unreal/World/SSLM_LightingLabUE58.py"
PIE = ROOT / "Tools/Unreal/World/SSLM_LightingLabPIEReviewUE.py"
DEFAULT_GAME = ROOT / "Game/Config/DefaultGame.ini"
SERVER_EXCLUSIONS = ROOT / "Build/Game/ServerPresentationExclusions.json"
CONTENT_MARKER = ROOT / "Game/Content/Development/LightingLab/README.md"


def main():
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    assert config["schema_version"] == 1
    assert config["engine_version"] == "5.8"
    assert config["formal_map"] == "/DBAWorldPack_Village/Maps/L_Village_Start"
    assert config["review_root"] == "/Game/Development/LightingLab"
    assert config["review_root"].startswith("/Game/Development/")
    assert config["output_relative"].startswith("Saved/Validation/")
    assert config["exposure_policy"]["mode"] == "inherit_formal_baseline"
    assert config["exposure_policy"]["assert_post_process_unchanged"] is True
    assert CONTENT_MARKER.exists(), "Lighting Lab开发目录说明缺失"
    assert 'DirectoriesToNeverCook=(Path="/Game/Development/LightingLab")' in DEFAULT_GAME.read_text(encoding="utf-8")
    exclusions = json.loads(SERVER_EXCLUSIONS.read_text(encoding="utf-8"))
    lighting_exclusion = next(item for item in exclusions["CookDirectories"] if item["PackagePath"] == "/Game/Development/LightingLab")
    assert lighting_exclusion["Profiles"] == "AllServers"
    assert lighting_exclusion["Availability"] == "Present"

    assert set(config["profiles"]) == {
        "SoftDaylight", "MorningWarm", "OvercastDiffuse",
        "GoldenHour", "Twilight", "Moonlit"
    }
    assert len(config["presets"]) == 11
    assert len(config["shadow_sweep"]["source_angles_deg"]) == 5
    assert config["shadow_sweep"]["source_angles_deg"] == [0.5357, 1.0, 2.0, 3.0, 4.0]
    assert len([key for key in config["presets"] if "ShadowAngle" in key]) == 5
    assert len([key for key in config["presets"] if "ShadowAngle" not in key]) == 6

    assert set(config["quality"]) == {"High", "Medium", "Low"}
    assert [config["quality"][key]["scalability"] for key in ("High", "Medium", "Low")] == [2, 1, 0]
    assert [config["quality"][key]["r.VolumetricFog"] for key in ("High", "Medium", "Low")] == [1, 1, 0]
    assert set(config["gi_modes"]) == {"Baseline", "LumenSoftware", "LumenHardware"}

    for key, profile in config["profiles"].items():
        assert 0.0 <= profile["sun_intensity_multiplier"] <= 1.5, key
        assert 0.5357 <= profile["sun_source_angle_deg"] <= 4.0, key
        assert 1000.0 <= profile["temperature_kelvin"] <= 12000.0, key
        assert 0.0 <= profile["fog_density"] <= 0.05, key
        assert 0.0 <= profile["max_opacity"] <= 1.0, key
        assert len(profile["fog_srgb"]) == 3 and all(0 <= c <= 255 for c in profile["fog_srgb"]), key
        assert len(profile["volumetric_albedo_srgb"]) == 3, key

    for path in (AUTHOR, PIE):
        source = path.read_text(encoding="utf-8")
        ast.parse(source, filename=str(path))
    author_source = AUTHOR.read_text(encoding="utf-8")
    pie_source = PIE.read_text(encoding="utf-8")
    assert "sys.path.insert(0, str(SCRIPT_DIR))" in author_source
    assert "sys.path.insert(0, str(SCRIPT_DIR))" in pie_source
    for required in ("author.check_formal()", "author.geometry_equal", "author.save_review", "author.load_formal_and_copy"):
        assert required in author_source, required
    assert "post_process_unchanged" in author_source
    assert "get_editor_property(\"intensity\")" in author_source
    assert "get_editor_property(\"fog_density\")" in author_source
    assert "project_ini_modified" in pie_source
    assert "existing_clear_weather_preset_applied" in pie_source
    assert "apply_weather_preset_on_authority" not in pie_source
    assert "pie.setup(run, quality, view)" in pie_source
    assert "execute_console_command" in pie_source
    assert "get_console_variable_int_value" in pie_source
    assert "pie.CAMERA = camera" not in pie_source
    assert "LightingLabSnapshot_" in pie_source
    assert "_Report.json" in pie_source
    assert "DefaultEngine.ini" not in pie_source
    print("SSLM Lighting Lab offline contract: PASS")


if __name__ == "__main__":
    main()
