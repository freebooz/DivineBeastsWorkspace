# -*- coding: utf-8 -*-
"""SSLM Lighting Lab PIE对照入口。

复用既有VillageMorningPIEReviewUE的地图白名单、地表绑定和固定相机；不重新应用天气预设。
GI模式仅在PIE中尝试设置CVar，并以读回值决定支持状态；不写项目INI。
"""
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

    # 复用既有PIE地图白名单、固定镜头和地表绑定，但不重新应用Clear天气；
    # 每份Lighting Lab副本继承同一正式地图天气快照，避免天气投影覆盖作者候选。
    pie.CAMERA = None
    pie.HELD.clear()
    pie.RUN = run
    world = pie.game()
    quality_settings = config["quality"][quality]
    for group in ["ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination", "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"]:
        u.SystemLibrary.execute_console_command(world, "sg." + group + "Quality " + str(quality_settings["scalability"]))
    for cvar_name, value in quality_settings.items():
        if cvar_name != "scalability":
            u.SystemLibrary.execute_console_command(world, cvar_name + " " + str(value))
    for command in ["t.MaxFPS 0", "r.VSync 0", "r.DynamicRes.OperationMode 0", "r.ScreenPercentage 100", "r.GPUCsvStatsEnabled 1"]:
        u.SystemLibrary.execute_console_command(world, command)
    camera = pie.set_camera(view)
    landscape = pie.bind_landscape(world)
    viewport = u.WidgetLayoutLibrary.get_viewport_size(world)
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
    if requirements_met:
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
    report_path = lab.author.OUT / report_name
    assert not report_path.exists(), "PIE报告已存在，拒绝覆盖：" + str(report_path)
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
        "weather_policy": "inherit_saved_map_weather_no_runtime_weather_preset_applied",
        "formal_merge_approved": False,
        "project_ini_modified": False
    }
    lab.author.write(report_name, report)
    print("SSLM_LIGHTING_LAB_PIE " + str(report))
    return report
