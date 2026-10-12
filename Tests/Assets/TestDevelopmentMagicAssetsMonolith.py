# -*- coding: utf-8 -*-
"""Monolith真实资产只读门禁：36项开发魔法、12套授权/Profile和6套VFX，缺失明确失败。
预期数值为独立测试夹具，不调用作者生成函数求期望；不写资产，不把存在性称作网络/视觉验收。
运行方式：Monolith editor.run_python execute_file；必须打开正式UE5.8工程。
"""
from pathlib import Path
import json
import unreal

HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")
EXPECTED = {"Fireball": (1200.0, 0.0, 0.0, 1, 0.0),
            "Frostbolt": (1600.0, 0.0, 0.0, 1, 0.0),
            "ArcaneMissiles": (1400.0, 18.0, 3.0, 3, 0.2)}
ROOT = "/Game/Development/DivineBeasts/Abilities/MagicTests"


def main():
    """全部收集错误后抛出；独占Saved报告不包含密码/凭据，也不覆盖既有导入报告。"""
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "DivineBeastsArena.uproject":
        raise RuntimeError("编辑器工程错误")
    errors, inspected = [], []
    for hero in HEROES:
        for spell, expected in EXPECTED.items():
            path = ROOT + "/DA_DBA_" + hero + "_Dev" + spell
            asset = unreal.load_asset(path)
            inspected.append(path)
            if not asset or asset.get_class().get_name() != "DivineBeastsDevelopmentMagicDefinition":
                errors.append("缺少真实魔法定义: " + path)
                continue
            if str(asset.get_editor_property("HeroDefinitionId")) != "Hero.Zodiac." + hero:
                errors.append("英雄身份不符: " + path)
            motion = asset.get_editor_property("ProjectileMotion")
            values = (float(motion.get_editor_property("SpeedCmPerSecond")),
                      float(motion.get_editor_property("HelixRadiusCm")),
                      float(motion.get_editor_property("HelixTurnsPerSecond")),
                      int(asset.get_editor_property("ProjectileCount")),
                      float(asset.get_editor_property("ProjectileIntervalSeconds")))
            if any(abs(a-b) > 0.0001 for a, b in zip(values, expected)):
                errors.append("运动/连发合同不符: " + path)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GA_DBA_" + hero + "_Dev" + spell)
            if cls is None or not isinstance(unreal.get_default_object(cls), unreal.DivineBeastsDevelopmentMagicAbility):
                errors.append("真实GAS派生/生成类错误: " + path)
    for spell in EXPECTED:
        icon = "/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev" + spell
        texture = unreal.load_asset(icon)
        if not isinstance(texture, unreal.Texture2D):
            errors.append("图标未导入: " + icon)
        for stage in ("Flight", "Impact"):
            path = "/DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NS_DBA_Dev" + spell + stage
            inspected.append(path)
            if not isinstance(unreal.load_asset(path), unreal.NiagaraSystem):
                errors.append("Niagara资产未制作: " + path)
    report = {"Project": str(project), "Inspected": inspected, "Errors": errors,
              "Passed": not errors, "NetworkVerified": False, "ManualVisualAccepted": False}
    output = project.parents[1] / "Saved/Validation/MagicHUD20261012/MagicAssets.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    if errors:
        raise RuntimeError("开发魔法资产未通过: " + json.dumps(errors, ensure_ascii=False))
    print(json.dumps({"Passed": True, "Report": str(output)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
