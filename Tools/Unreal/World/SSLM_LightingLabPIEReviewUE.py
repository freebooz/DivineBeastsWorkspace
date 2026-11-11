# -*- coding: utf-8 -*-
"""SSLM Lighting Lab PIE对照入口。

复用既有VillageMorningPIEReviewUE的天气、地表绑定和固定相机。
GI模式仅在PIE中尝试设置CVar，并以读回值决定支持状态；不写项目INI。
"""
import re

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

    # 既有PIE助手会校验地图位于LightingLab白名单，并绑定现有Village地表/天气。
    pie.setup(run, quality, view)
    world = pie.game()
    mode = config["gi_modes"][gi_mode]
    required_names = list(mode["requires"])
    expected_names = list(mode["expected"])
    command_names = [item[0] for item in mode["commands"]]
    names = sorted(set(required_names + expected_names + command_names))
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
    if gi_mode == "Baseline":
        status = "observed"
    elif not requirements_met:
        status = "unsupported"
    elif expected_met:
        status = "readback_passed"
    else:
        status = "readback_mismatch"

    # 复用既有快照，保留实际雾、方向光、相机、天气风和基础GI CVar读值。
    pie.snapshot("LightingLab_" + gi_mode)
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
        "cvars_before": before,
        "cvars_after": after,
        "world": world.get_path_name(),
        "engine": u.SystemLibrary.get_engine_version(),
        "formal_merge_approved": False,
        "project_ini_modified": False
    }
    lab.author.write(run + "_LightingLab_" + gi_mode + ".json", report)
    print("SSLM_LIGHTING_LAB_PIE " + str(report))
    return report
