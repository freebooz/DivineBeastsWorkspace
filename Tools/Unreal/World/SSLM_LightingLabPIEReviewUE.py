# -*- coding: utf-8 -*-
"""SSLM Lighting Lab PIE对照入口。

复用既有VillageMorningPIEReviewUE的地图白名单、Clear天气基线、地表绑定和固定相机。
GI模式仅在PIE中尝试设置CVar，并以读回值决定支持状态；不写项目INI。
"""
import json
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import unreal as u
import SSLM_LightingLabUE58 as lab
import VillageMorningPIEReviewUE as pie


def _read_cvars(names):
    return {name: u.SystemLibrary.get_console_variable_int_value(name) for name in names}


def setup(run, quality="High", view="LakeShore", gi_mode="Baseline"):
    config = lab.configure()
    assert re.fullmatch(r"[A-Za-z0-9_-]{1,80}", run), "运行记录名只能使用ASCII字母/数字/下划线/连字符"
    assert quality in config["quality"], "未知质量档：" + quality
    assert view in config["camera"]["views"], "未知固定视角：" + view
    assert gi_mode in config["gi_modes"], "未知GI模式：" + gi_mode

    # 既有PIE助手显式应用已验证的Clear天气定义，固定天气/风场基线，再设置质量与固定相机。
    # Clear是测试基线；不同光照profile只在保存的隔离地图上改变，不创建第二套天气链。
    setup_path = lab.author.OUT / (run + "_Setup.json")
    snapshot_path = lab.author.OUT / (run + "_LightingLabSnapshot_" + gi_mode + ".json")
    report_path = lab.author.OUT / (run + "_LightingLab_" + gi_mode + "_Report.json")
    for evidence_path in (setup_path, snapshot_path, report_path):
        if evidence_path.exists():
            raise AssertionError("PIE证据已存在，拒绝覆盖：" + str(evidence_path))
    pie.setup(run, quality, view)
    assert setup_path.exists(), "既有PIE助手未生成Setup证据"
    setup_report = json.loads(setup_path.read_text(encoding="utf-8"))
    landscape = setup_report["landscape"]
    world = pie.game()
    camera = pie.set_camera(view)
    viewport = u.WidgetLayoutLibrary.get_viewport_size(world)
    quality_settings = config["quality"][quality]
    mode = config["gi_modes"][gi_mode]
    required_names = list(mode["requires"])
    expected_names = list(mode["expected"])
    command_names = [item[0] for item in mode["commands"]]
    observed_names = [
        "r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod",
        "r.RayTracing", "r.Lumen.HardwareRayTracing"
    ]
    quality_names = [name for name in quality_settings if name != "scalability"]
    names = sorted(set(required_names + expected_names + command_names + observed_names + quality_names))
    before = _read_cvars(names)

    requirements_met = all(before[name] == int(value) for name, value in mode["requires"].items())
    commands_applied = False
    if requirements_met and mode["commands"]:
        for name, value in mode["commands"]:
            u.SystemLibrary.execute_console_command(world, name + " " + str(int(value)))
        commands_applied = True

    after = _read_cvars(names)
    expected_met = requirements_met and all(
        after[name] == int(value) for name, value in mode["expected"].items()
    )
    quality_readback = {name: after[name] for name in quality_names}
    quality_readback_passed = all(
        after[name] == int(value) for name, value in quality_settings.items()
        if name != "scalability"
    )
    if gi_mode == "Baseline":
        status = "observed"
    elif not requirements_met:
        status = "unsupported"
    elif expected_met:
        status = "readback_passed"
    else:
        status = "readback_mismatch"

    # 每次运行使用唯一证据名；先保存既有快照，再写入摘要报告，避免互相覆盖。
    snapshot_suffix = "LightingLabSnapshot_" + gi_mode
    report_name = run + "_LightingLab_" + gi_mode + "_Report.json"
    pie.snapshot(snapshot_suffix)
    report = {
        "status": status,
        "run": run,
        "quality": quality,
        "view": view,
        "gi_mode": gi_mode,
        "description": mode["description"],
        "commands_applied": commands_applied,
        "requirements_met": requirements_met,
        "expected_readback_passed": expected_met,
        "quality_readback": quality_readback,
        "quality_readback_passed": quality_readback_passed,
        "cvars_before": before,
        "cvars_after": after,
        "world": world.get_path_name(),
        "engine": u.SystemLibrary.get_engine_version(),
        "camera": camera,
        "landscape": landscape,
        "viewport": [viewport.x, viewport.y],
        "weather_policy": "existing_clear_weather_preset_applied",
        "gi_execution_verified": False,
        "validation_scope": "CVar readback only; actual render path, GPU cost and visual quality require separate UE5.8 evidence",
        "formal_merge_approved": False,
        "project_ini_modified": False
    }
    lab.author.write(report_name, report)
    print("SSLM_LIGHTING_LAB_PIE " + str(report))
    return report
