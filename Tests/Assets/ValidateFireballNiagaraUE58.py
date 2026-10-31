# -*- coding: utf-8 -*-
"""标准火球接入只读原生门禁：检查真实保存依赖、唯一绑定及50cm材质合同。

仅由Monolith在SSLM UE5.8执行，不导入作者、不改资产、不替代实际粒子尺寸、
CPU/GPU编译、Cook、联机或人工视觉验证。先记录未接线的RED，再检查保存后的GREEN。
"""
import json
from pathlib import Path
import sys
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/Unreal/VFX/SpellProjectiles"))
from SpellPresentationContract import load_manifest


def validate(report_path=None, raise_on_error=True):
    """返回真实原生错误集合；缺资产、旧绑定或尺寸错误都不能用跳过检查变绿。"""
    if Path(u.Paths.get_project_file_path()).resolve() != ROOT / "Game/DivineBeastsArena.uproject":
        raise RuntimeError("不是SSLM主工程")
    if not u.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("不是锁定UE5.8")
    base = "/DBAPresentationPack_Core/VFX/SpellProjectiles"
    material_root = base + "/Materials/Fireball/MI_DBA_Fireball_"
    stage_materials = {
        "Cast": ["Core", "Trail", "Ember", "AirRipple"],
        "Flight": ["Volume", "Core", "Trail", "AirRipple"],
        "Impact": ["Impact", "Ember", "AirRipple"],
    }
    registry = u.AssetRegistryHelpers.get_asset_registry()
    options = u.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                              include_soft_package_references=True)
    errors, stages = [], []
    data = load_manifest()
    for stage, materials in stage_materials.items():
        rows = [row for row in data["bindings"] if row["spell"] == "Fireball" and row["stage"] == stage]
        if len(rows) != 1:
            errors.append("当前火球阶段没有唯一合同：" + stage)
            continue
        row = rows[0]
        system, definition = u.load_asset(row["system"]), u.load_asset(row["definition"])
        if not isinstance(system, u.NiagaraSystem) or definition is None:
            errors.append("当前资产缺失：" + stage)
            continue
        reference = definition.get_editor_property("niagara_system")
        if not reference or reference.get_path_name().split(".")[0] != row["system"]:
            errors.append("Definition不再引用当前唯一系统：" + stage)
        dependencies = {str(p) for p in registry.get_dependencies(u.Name(row["system"]), options)}
        for name in materials:
            if material_root + name not in dependencies:
                errors.append(stage + "没有保存材质依赖：" + name)
        if base + "/Niagara/NM_DBA_FireballMaterialParameters" not in dependencies:
            errors.append(stage + "没有公共材质参数适配模块")
        # 编译通过不能证明Emitter已接进系统Spawn/Update图；本次真实缺接线回归。
        node_class = u.load_class(None, "/Script/NiagaraEditor.NiagaraNodeEmitter")
        nodes = [n for n in u.ObjectIterator(node_class)
                 if n.get_path_name().startswith(system.get_path_name() + ":SystemSpawnScript.")]
        if len(nodes) != 8:
            errors.append(stage + "系统执行图未连接全部4个Emitter：" + str(len(nodes)))
        stages.append({"stage": stage, "system": row["system"], "dependencies": sorted(dependencies)})
    volume = u.load_asset(material_root + "Volume")
    if not isinstance(volume, u.MaterialInstanceConstant):
        errors.append("火球体积实例缺失")
    else:
        radius = u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(volume, "RadiusCm")
        if abs(radius - 25.0) > .001:
            errors.append("球头半径不是25cm：" + str(radius))
    result = {"Passed": not errors, "Errors": errors, "Stages": stages,
              "PhysicalParticleSizeVerified": False, "CookVerified": False,
              "RuntimeVerified": False, "ManualVisualAccepted": False}
    if report_path:
        path = Path(report_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if errors and raise_on_error:
        raise RuntimeError("火球原生接入门禁失败：" + "; ".join(errors))
    return result


if __name__ == "__main__":
    print(json.dumps(validate(ROOT / "Saved/Validation/FireballNiagara20261031/NativeGate.json"), ensure_ascii=False))
