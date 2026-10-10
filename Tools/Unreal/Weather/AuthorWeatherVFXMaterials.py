#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 Niagara三种真实粒子材质生成器。

在锁定UE Editor运行。生成的每个UMaterial必须具有TextureSample、ParticleColor、
Multiply及Emissive/Opacity实际图连接。默认只读，显式
WEATHER_VFX_MATERIAL_MODE=apply 才创建，不允许覆盖现有资产。
仅创建材质；Niagara实际系统/发射器由Monolith基于WeatherNiagaraAuthoringSpec制作。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SPEC = ROOT / "Tools/Unreal/Weather/WeatherNiagaraAuthoringSpec_V1.json"
MODE = os.getenv("WEATHER_VFX_MATERIAL_MODE", "inspect").strip().lower()
DEST = "/GamePlatformVFX/Weather/Materials"
JOBS = (
    ("M_GP_VFX_Weather_Rain", "rain", 0.63),
    ("M_GP_VFX_Weather_Snow", "snow", 0.92),
    ("M_GP_VFX_Weather_Splash", "splash", 0.76),
)


def execute():
    spec = json.loads(SPEC.read_text(encoding="utf-8"))
    if spec.get("schemaVersion") != 1:
        raise RuntimeError("Niagara系统节点规格版本错误")
    texture_paths = spec["materialInputTextures"]
    for name, texture_key, opacity_scale in JOBS:
        target = DEST + "/" + name
        print("PARTICLE_MATERIAL_PLAN", target, texture_paths[texture_key], opacity_scale)
    if MODE == "inspect":
        print("WEATHER_VFX_MATERIAL_INSPECT_ONLY：没有创建uasset")
        return
    if MODE != "apply":
        raise ValueError("WEATHER_VFX_MATERIAL_MODE只允许inspect/apply")
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须通过真实UE5.8编辑器的PythonScriptPlugin运行") from exc
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_api = unreal.MaterialEditingLibrary
    for name, key, opacity in JOBS:
        path = texture_paths[key]
        if not assets.does_asset_exist(path):
            raise FileNotFoundError(f"缺少已导入Texture2D：{path}")
        if assets.does_asset_exist(DEST + "/" + name):
            raise FileExistsError(f"已有Niagara材质，拒绝覆盖：{name}")
    if not assets.does_directory_exist(DEST):
        assets.make_directory(DEST)

    def link(a, b, pin, out=""):
        if not material_api.connect_material_expressions(a, out, b, pin):
            raise RuntimeError(f"材质表达式连接失败：{a.get_name()} -> {b.get_name()} {pin}")

    for name, key, opacity in JOBS:
        material = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
        if not isinstance(material, unreal.Material):
            raise RuntimeError(f"创建真实Niagara材质失败：{name}")
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("two_sided", True)

        sample = material_api.create_material_expression(
            material, unreal.MaterialExpressionTextureSampleParameter2D, -850, 20)
        sample.set_editor_property("parameter_name", unreal.Name("ParticleTexture"))
        sample.set_editor_property("texture", assets.load_asset(texture_paths[key]))
        vertex = material_api.create_material_expression(
            material, unreal.MaterialExpressionParticleColor, -850, 370)
        rgb = material_api.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -470, 80)
        link(sample, rgb, "A", "RGB")
        link(vertex, rgb, "B", "RGB")
        if not material_api.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError("未连接Niagara材质Emissive输出")

        alpha_mul = material_api.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -460, 410)
        link(sample, alpha_mul, "A", "A")
        link(vertex, alpha_mul, "B", "A")
        scalar = material_api.create_material_expression(
            material, unreal.MaterialExpressionScalarParameter, -860, 670)
        scalar.set_editor_property("parameter_name", unreal.Name("OpacityScale"))
        scalar.set_editor_property("default_value", float(opacity))
        final_alpha = material_api.create_material_expression(
            material, unreal.MaterialExpressionMultiply, -150, 400)
        link(alpha_mul, final_alpha, "A")
        link(scalar, final_alpha, "B")
        if not material_api.connect_material_property(final_alpha, "", unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError("未连接Niagara材质Opacity输出")

        errors = list(material_api.recompile_material(material))
        if any("error" in str(x).lower() for x in errors):
            raise RuntimeError(f"粒子材质Shader编译失败：{name} {errors}")
        if not assets.save_loaded_asset(material, only_if_is_dirty=False):
            raise RuntimeError(f"Niagara粒子材质未保存：{name}")
        loaded = assets.load_asset(DEST + "/" + name)
        if not isinstance(loaded, unreal.Material):
            raise RuntimeError(f"Niagara粒子材质回读失败：{name}")
        print("NIAGARA_MATERIAL_SAVED", loaded.get_path_name())


if __name__ == "__main__":
    execute()
