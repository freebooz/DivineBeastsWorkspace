#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""正式鸟群WorldDefinition兼容扩展：只追加4种可选GPU参数及旧FlightMode值3—5。
仅在锁定UE5.8编辑器内执行；保留三种Definition原身份、Niagara系统和旧七字段默认值。
执行前须已有真实项目已编译反射类型；脚本不创建/仿造.uasset，未满足条件即失败。
"""
from __future__ import annotations

from pathlib import Path
import json
import unreal

PROJECT = "/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Definitions"
SPECIES = ("Seagull", "Swallow", "Eagle")
NEW_FIELDS = (
    ("User.SeparationWeight", "Float", 0.0, 4.0),
    ("User.AlignmentWeight", "Float", 0.0, 4.0),
    ("User.CohesionWeight", "Float", 0.0, 4.0),
    ("User.GlideBlend", "Float", 0.0, 1.0),
)
OLD_NAMES = {
    "User.BirdCount", "User.FlockRadius", "User.FlockHeight",
    "User.FlightSpeed", "User.FlockSeed", "User.FlightMode", "User.WindInfluence"
}

def validate_environment():
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前UE编辑器不是授权的神兽联盟主工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("本编辑器不是锁定的UE5.8版本")

def field_rule(name, typ, low, high):
    rule = unreal.GamePlatformVFXParameterRule()
    # 类型枚举在旧Definition源码中已经对Integer/Float单独映射。
    rule.set_editor_property("name", unreal.Name(name))
    rule.set_editor_property(
        "type", getattr(unreal.GamePlatformVFXParameterType, typ.upper()))
    rule.set_editor_property("required", False)
    rule.set_editor_property("min_value", float(low))
    rule.set_editor_property("max_value", float(high))
    return rule

def main():
    validate_environment()
    assets = unreal.EditorAssetLibrary
    prepared = []
    # 所有资源先只读预检，防止创建缺一半的物种目录。
    for species in SPECIES:
        name = PROJECT + "/DA_DBA_VFX_BirdFlock_" + species
        obj = assets.load_asset(name)
        if not isinstance(obj, unreal.GamePlatformVFXWorldDefinition):
            raise RuntimeError("正式WorldDefinition缺失或类型不正确：" + name)
        schema = obj.get_editor_property("parameter_schema")
        existing = list(schema.get_editor_property("rules"))
        by_name = {str(x.get_editor_property("name")): x for x in existing}
        if len(by_name) != len(existing) or not OLD_NAMES.issubset(by_name.keys()):
            raise RuntimeError("旧七参数合同丢失或重复：" + name)
        for field_name, _, _, _ in NEW_FIELDS:
            if field_name in by_name:
                # 增量脚本只给未存在的字段新增合法Rule，不任意覆盖策划已经调整的值。
                print("BIRD_RULE_PRESENT", species, field_name)
        prepared.append((species, name, obj, schema, existing, by_name))

    changed = []
    for species, name, obj, schema, existing, by_name in prepared:
        rules = list(existing)
        # FlightMode旧0/1/2保留，增加3=SplineOnce,4=SplineLoop,5=SplinePingPong。
        old_mode = by_name["User.FlightMode"]
        old_mode.set_editor_property("max_value", 5.0)
        for field_name, typ, low, high in NEW_FIELDS:
            if field_name not in by_name:
                rules.append(field_rule(field_name, typ, low, high))
        schema.set_editor_property("rules", rules)
        schema.set_editor_property("max_override_count", max(11, len(rules)))
        obj.set_editor_property("parameter_schema", schema)
        if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
            raise RuntimeError("真实Niagara鸟群Definition保存失败：" + name)
        check = obj.get_editor_property("parameter_schema")
        names = {str(x.get_editor_property("name")) for x in check.get_editor_property("rules")}
        if not OLD_NAMES.issubset(names) or not {f[0] for f in NEW_FIELDS}.issubset(names):
            raise RuntimeError("保存后参数校验失败：" + name)
        changed.append({"species": species, "path": name,
            "schema_fields": sorted(names), "schema_count": len(names)})
        print("BIRD_SPLINE_DEFINITION_SAVED", name, len(names))

    workspace = Path(unreal.Paths.get_project_file_path()).resolve().parent.parent
    evidence = workspace/"Saved/Validation/BirdFlockSplineFlight"
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence/"WorldDefinitionsAuthoring.json").write_text(
        json.dumps({"status":"assets_saved","definitions":changed,
                    "manual_visual_approval":"pending",
                    "niagara_execution":"not_proven_by_schema_alone"},
                   ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("BIRD_SPLINE_DEFINITION_ASSETS_READY", len(changed))

if __name__ == "__main__":
    main()
