# -*- coding: utf-8 -*-
"""Niagara灰色方片排障第二道隔离门禁：使用静态网格显示同四份Material。

目的：把材质本身错误（静态网格也回退）和Niagara SpriteRenderer错误
（静态网格正确但粒子回退）区分开。仅新增独立测试UMAP，不改正式资产。
须真实D3D12游戏截图验收，资产保存和Unreal API回读不算通过。
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
BASE = "/Game/Development/FallingFoliage/Probes"
MAP = BASE + "/L_DBA_FoliageShaderMeshControlReview"
REPORT = ROOT / "Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/ShaderMeshControlAudit.json"
ED = unreal.EditorAssetLibrary
LEVEL = unreal.EditorLevelLibrary
SOURCE_MESH = "/Engine/BasicShapes/Plane"
SHADERS = {
    "Solid": BASE + "/M_DBA_FoliageShaderIsolation_Solid",
    "UV0": BASE + "/M_DBA_FoliageShaderIsolation_UV0",
    "Opacity": BASE + "/M_DBA_FoliageShaderIsolation_Opacity",
    "PBR": BASE + "/MI_DBA_PearPetal_MaskSamplerProbe",
}
audit = {"map": MAP, "status": "pending", "materialBindings": {},
         "errors": [], "gpuVisualPassed": False}

def require(value, message):
    """发现引擎/资源不符时停止，禁止把错误当作渲染通过。"""
    if not value:
        raise RuntimeError(message)

def spawn(cls, label, loc, rotation=None):
    """使用真实UE EditorLevelLibrary创建场景Actor。"""
    actor = LEVEL.spawn_actor_from_class(
        cls, unreal.Vector(*loc), rotation or unreal.Rotator(0, 0, 0))
    require(actor is not None, "Actor生成失败：" + label)
    actor.set_actor_label("ShaderMeshControl_" + label)
    return actor

try:
    require(str(unreal.Paths.get_project_file_path()).replace("\\", "/").endswith(
            "/DivineBeastsArena.uproject"), "非神兽联盟工作空间")
    require(str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."),
            "非UE5.8引擎")
    require(not ED.does_asset_exist(MAP), "测试地图已存在，拒绝覆盖")
    quad = ED.load_asset(SOURCE_MESH)
    require(isinstance(quad, unreal.StaticMesh), "引擎Plane测试网格缺失")
    materials = {}
    for key, path in SHADERS.items():
        m = ED.load_asset(path)
        require(isinstance(m, unreal.MaterialInterface), "不能加载静态网格对照Material："+path)
        materials[key] = m
    require(LEVEL.new_level(MAP), "不能生成独立材质静态网格审核地图")
    for name, y in zip(("Solid", "UV0", "Opacity", "PBR"),
                       (-300, -100, 100, 300)):
        plane = spawn(unreal.StaticMeshActor, name, (145, y, 145),
                      unreal.Rotator(pitch=90, yaw=0, roll=0))
        part = plane.get_component_by_class(unreal.StaticMeshComponent)
        require(part is not None, "缺少静态网格组件")
        part.set_static_mesh(quad)
        part.set_material(0, materials[name])
        plane.set_actor_scale3d(unreal.Vector(1.5, 1.5, 1))
        require(part.get_material(0) == materials[name], "材质槽回读不正确："+name)
        audit["materialBindings"][name] = materials[name].get_path_name()
    floor = spawn(unreal.StaticMeshActor, "Floor", (0,0,-5))
    floor.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(quad)
    floor.set_actor_scale3d(unreal.Vector(25,25,1))
    sun = spawn(unreal.DirectionalLight, "Sun", (300,0,950),
                unreal.Rotator(pitch=-55,yaw=30,roll=0))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property(
        "intensity", 40.0)
    spawn(unreal.SkyLight, "Sky", (0,0,700))
    spawn(unreal.SkyAtmosphere, "Atmosphere", (0,0,0))
    view = unreal.Rotator(pitch=0, yaw=0, roll=0)
    spawn(unreal.PlayerStart, "PlayerStart", (-425,0,145), view)
    spawn(unreal.CameraActor, "Camera", (-425,0,145), view)
    require(LEVEL.save_current_level(), "实际UMAP保存失败")
    audit["status"] = "mesh_control_map_saved_visual_pending"
    audit["expectedLeftToRight"] = ["Solid红色板", "UV0彩色渐变板",
                                   "Opacity独立梨花轮廓", "PBR受光梨花轮廓"]
    print("FOLIAGE_SHADER_MESH_CONTROL_CREATED", MAP, len(materials), flush=True)
except Exception as exc:
    audit["status"] = "failed"
    audit["errors"].append(str(exc))
    print("FOLIAGE_SHADER_MESH_CONTROL_ERROR", str(exc), flush=True)
    raise
finally:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(audit, ensure_ascii=False, indent=2)+"\n", encoding="utf8")
