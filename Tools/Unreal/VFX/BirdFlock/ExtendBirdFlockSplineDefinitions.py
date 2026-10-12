#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""项目鸟群高级WorldDefinition（世界特效定义）真实UE编辑器作者。

只复制三种已发布@1定义，制作独立_Spline变体并分配明确的@2逻辑身份。
原Definition@1、原Niagara、原七项参数及现有材质保持原位，不允许扩展失败污染旧版本。
必须等真实Niagara _Spline系统完成GPU模块图/样条DI回读后执行。
脚本仅允许UE5.8独占编辑器真实写入，不把JSON/源文件改名为uasset。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import unreal  # 只有在UE5.8的PythonScriptPlugin（编辑器Python插件）中可导入

PROJECT = "/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Definitions"
NIAGARA = "/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara"
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
        raise RuntimeError("不是已授权的神兽联盟主工程，拒绝编辑定义。")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("必须使用锁定UE5.8版本。")
    return Path(project).resolve().parent.parent


def new_parameter(name, value_type, minimum, maximum):
    rule = unreal.GamePlatformVFXParameterRule()
    rule.set_editor_property("name", unreal.Name(name))
    rule.set_editor_property("type", getattr(unreal.GamePlatformVFXParameterType, value_type.upper()))
    rule.set_editor_property("required", False)
    rule.set_editor_property("min_value", float(minimum))
    rule.set_editor_property("max_value", float(maximum))
    return rule


def ensure_v2_definition(assets, species, evidence):
    source = PROJECT + "/DA_DBA_VFX_BirdFlock_" + species
    destination = source + "_Spline"
    niagara_path = NIAGARA + "/NS_DBA_BirdFlock_" + species + "_Spline"
    original = assets.load_asset(source)
    target_system = assets.load_asset(niagara_path)
    if not isinstance(original, unreal.GamePlatformVFXWorldDefinition):
        raise RuntimeError("旧@1正式WorldDefinition丢失：" + source)
    if not isinstance(target_system, unreal.NiagaraSystem):
        raise RuntimeError("新Spline Niagara变体未实际生成/保存：" + niagara_path)

    if assets.does_asset_exist(destination):
        target = assets.load_asset(destination)
        if not isinstance(target, unreal.GamePlatformVFXWorldDefinition):
            raise RuntimeError("新@2定义路径被其他类型占用：" + destination)
    else:
        target = assets.duplicate_asset(source, destination)
        if not isinstance(target, unreal.GamePlatformVFXWorldDefinition):
            raise RuntimeError("UE复制真实DataAsset失败：" + destination)

    # 绝不在原@1上改变FlightMode白名单；本@2专门指向独立Spline变体。
    schema = target.get_editor_property("parameter_schema")
    rules = list(schema.get_editor_property("rules"))
    names = {str(rule.get_editor_property("name")): rule for rule in rules}
    if len(names) != len(rules) or not OLD_NAMES.issubset(names):
        raise RuntimeError("原参数合同缺失或重复，不能制作新变体：" + destination)
    mode = names["User.FlightMode"]
    mode.set_editor_property("max_value", 5.0)
    for name, value_type, low, high in NEW_FIELDS:
        if name not in names:
            rules.append(new_parameter(name, value_type, low, high))
    schema.set_editor_property("rules", rules)
    schema.set_editor_property("max_override_count", max(11, len(rules)))
    target.set_editor_property("parameter_schema", schema)
    target.set_editor_property("niagara_system", target_system)

    identity = unreal.GamePlatformId()
    identity.set_editor_property("namespace", "presentation.dba.environment.birdflock")
    identity.set_editor_property("name", species.lower())
    identity.set_editor_property("logical_version", 2)
    target.set_editor_property("logical_id", identity)

    # 维持SchemaVersion（表结构版本），只更新内容修订避免与旧@1产物混淆。
    content_version = target.get_editor_property("data_version")
    content_version.set_editor_property("content_revision", 2)
    target.set_editor_property("data_version", content_version)

    if not assets.save_loaded_asset(target, only_if_is_dirty=False):
        raise RuntimeError("新@2真实定义保存失败：" + destination)
    verify = assets.load_asset(destination)
    if not isinstance(verify, unreal.GamePlatformVFXWorldDefinition):
        raise RuntimeError("新@2定义重新加载失败：" + destination)
    verify_names = {str(rule.get_editor_property("name"))
                    for rule in verify.get_editor_property("parameter_schema").get_editor_property("rules")}
    if not OLD_NAMES.issubset(verify_names) or not {x[0] for x in NEW_FIELDS}.issubset(verify_names):
        raise RuntimeError("新@2定义Schema回读失败：" + destination)
    evidence.append({"species": species, "definition": destination,
                     "niagara": niagara_path, "logical_version": 2,
                     "schema_names": sorted(verify_names),
                     "niagara_gpu_live": "not_yet_validated"})


def main():
    root = validate_environment()
    assets = unreal.EditorAssetLibrary
    evidence = []
    originals = [
        root / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/"
               "Content/VFX/Environment/BirdFlock/Definitions" / ("DA_DBA_VFX_BirdFlock_" + name + ".uasset")
        for name in SPECIES
    ]
    hashes_before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    for species in SPECIES:
        ensure_v2_definition(assets, species, evidence)
    hashes_after = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    if hashes_before != hashes_after:
        raise RuntimeError("旧@1鸟群定义文件在高级版本制作时发生改动，必须人工审查恢复。")

    output = root / "Saved/Validation/BirdFlockSplineFlight/WorldDefinitionsAuthoring.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps({"status": "new_v2_definitions_saved_pending_runtime",
                    "old_definition_hashes_unchanged": True, "definitions": evidence,
                    "schema_only_cannot_prove_gpu": True, "manual_visual_approval": "pending"},
                   ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("BIRD_SPLINE_V2_DEFINITIONS_SAVED", len(evidence))


if __name__ == "__main__":
    main()

