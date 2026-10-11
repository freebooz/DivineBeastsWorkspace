#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8通用柔边飘雪Sprite真实资产制作，仅创建全新Texture2D和MaterialInstance。

职责：GamePlatformVFX平台层环境降雪通用材质；项目角色和服务器不硬引用。
依赖：GenerateSnowSoftFlake.py的原始RGBA PNG和SHA-256 manifest，已经存在的
M_GP_VFX_Weather_Snow（原通用雪材质）作为项目兼容父类。
安全：默认inspect只读；SNOW_SOFT_VFX_AUTHOR_MODE=apply才允许UE Editor导入源PNG、
创建材质实例并将原材质的ParticleTexture替换成柔边图。任何已存在不符的同名资产拒绝修改，
不覆盖原M_GP_VFX_Weather_Snow、不触碰原NS_GP_Weather_Snow或现有增强NS。
真正的Niagara近远景renderer接线在UE Monolith进行并独立验证/编译/保存。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("SnowSoftFlakeSourceManifest.json")
SOURCE = "/GamePlatformVFX/Weather/Textures/T_GP_Snow_SoftFlake"
MASTER = "/GamePlatformVFX/Weather/Materials/M_GP_VFX_Weather_Snow"
INSTANCE = "/GamePlatformVFX/Weather/Materials/MI_GP_VFX_Weather_SnowSoft"
MODE = os.getenv("SNOW_SOFT_VFX_AUTHOR_MODE", "inspect").strip().lower()


def validate_source() -> Path:
    spec = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if spec.get("schemaVersion") != 1 or spec.get("textureAsset") != SOURCE:
        raise ValueError("雪源Manifest版本或UE资产身份不一致")
    raw = (ROOT / spec["sourcePath"]).resolve()
    if (not raw.is_relative_to(ROOT) or not raw.is_file() or raw.suffix.lower() != ".png"
            or hashlib.sha256(raw.read_bytes()).hexdigest() != spec["sha256"]):
        raise RuntimeError("柔边雪源RGBA PNG缺失、越界或SHA256被修改")
    if spec["textureSize"] != [512, 512] or spec["mode"] != "RGBA":
        raise ValueError("柔边Sprite通道/尺寸必须512RGBA")
    return raw


def main() -> None:
    source = validate_source()
    print("SNOW_SOFT_AUTHOR_PLAN", MODE, str(source), SOURCE, INSTANCE)
    if MODE == "inspect":
        return
    if MODE != "apply":
        raise ValueError("仅允许inspect和apply模式")
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("禁止普通Python伪造.uasset；需正式UnrealEditor") from exc
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前Editor没有载入正式神兽联盟工程")

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    if not assets.does_asset_exist(MASTER):
        raise FileNotFoundError("平台基础粒子母材质不存在：" + MASTER)
    parent = assets.load_asset(MASTER)
    if not isinstance(parent, unreal.Material):
        raise TypeError("平台基础雪粒子必须为UMaterial")
    created_texture = False

    # 先完整检查同名资产；任何类型冲突必须在修改引擎资源前失败。
    if assets.does_asset_exist(SOURCE) and not isinstance(assets.load_asset(SOURCE), unreal.Texture2D):
        raise TypeError("已存在同名Texture2D外的资产，拒绝修改：" + SOURCE)
    if assets.does_asset_exist(INSTANCE) and not isinstance(
            assets.load_asset(INSTANCE), unreal.MaterialInstanceConstant):
        raise TypeError("已存在同名非材质实例，拒绝覆盖：" + INSTANCE)

    if not assets.does_asset_exist(SOURCE):
        folder, asset_name = SOURCE.rsplit("/", 1)
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        if not task.get_editor_property("imported_object_paths"):
            raise RuntimeError("UE未创建真实柔边Sprite纹理")
        created_texture = True
    texture = assets.load_asset(SOURCE)
    if not isinstance(texture, unreal.Texture2D):
        raise TypeError("UE柔边Sprite原生Texture2D不可回读")
    if created_texture:
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_DEFAULT)
        if not assets.save_loaded_asset(texture, only_if_is_dirty=False):
            raise RuntimeError("UE柔边Sprite Texture2D保存失败")
        print("SNOW_SOFT_TEXTURE_SAVED", SOURCE)

    if not assets.does_asset_exist(INSTANCE):
        folder, name = INSTANCE.rsplit("/", 1)
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        instance = tools.create_asset(
            name, folder, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("创建真实UMaterialInstanceConstant失败")
        if not lib.set_material_instance_parent(instance, parent):
            # 部分UE反射API Setter无返回值，仍通过后续Parent回读判断。
            if instance.get_editor_property("parent") != parent:
                raise RuntimeError("无法设置Sprite MaterialInstance父材质")
        if not lib.set_material_instance_texture_parameter_value(
                instance, "ParticleTexture", texture):
            if lib.get_material_instance_texture_parameter_value(
                    instance, "ParticleTexture") != texture:
                raise RuntimeError("粒子柔边贴图参数覆盖失败")
        lib.set_material_instance_scalar_parameter_value(instance, "OpacityScale", .72)
        if not assets.save_loaded_asset(instance, only_if_is_dirty=False):
            raise RuntimeError("柔边雪MaterialInstance保存失败")
        print("SNOW_SOFT_MATERIAL_INSTANCE_SAVED", INSTANCE)

    instance = assets.load_asset(INSTANCE)
    if not isinstance(instance, unreal.MaterialInstanceConstant):
        raise TypeError("UE已保存雪材质实例类型不正确")
    if instance.get_editor_property("parent") != parent:
        raise RuntimeError("柔边雪材质没有继承原平台雪粒子Material")
    linked = lib.get_material_instance_texture_parameter_value(instance, "ParticleTexture")
    if linked != texture:
        raise RuntimeError("雪粒子材质尚未生效柔边Sprite Texture2D：" + str(linked))
    print("SNOW_SOFT_VFX_ASSET_VERIFY_OK", SOURCE, INSTANCE,
          "parent", MASTER, "texture", linked.get_path_name())


if __name__ == "__main__":
    main()
