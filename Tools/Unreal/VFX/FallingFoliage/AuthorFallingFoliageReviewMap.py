#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8自然飘落视觉审核关卡作者工具（只用于Editor/人工验收）。
新建独立的开发地图，不修改当前真实选角地图、正在打开的编辑器UI或正式服务器世界。
使用真实NiagaraActor加载现有桃花 Niagara System，保留原项目层资产身份。
默认inspect；仅FOLIAGE_REVIEW_MAP_MODE=apply由隔离编辑器命令行真实执行才生成.umap。
"""
from __future__ import annotations
import os

MAP_PATH = "/Game/Development/FallingFoliage/Maps/L_DBA_FallingFoliageReview"
SYSTEM_PATH = ("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/"
               "Niagara/NS_DBA_FallingFoliage_Peach")
ANCHOR_PATH = ("/DBAFrontEndPack/CharacterPreview/Environment/Blueprints/"
               "BP_DBA_FoliageGroundAnchor")
GROUND_MESH = "/Engine/BasicShapes/Plane"
MODE = os.getenv("FOLIAGE_REVIEW_MAP_MODE", "inspect").strip().lower()


def execute() -> None:
    print("FOLIAGE_REVIEW_MAP_PLAN", MAP_PATH, SYSTEM_PATH)
    if MODE == "inspect":
        return
    if MODE not in ("apply", "calibrate"):
        raise ValueError("只允许inspect/apply/calibrate，不能在普通关卡上操作")

    import unreal  # type: ignore
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("禁止在其他项目创建审核地图：" + project)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("必须使用锁定UE5.8编辑器")

    assets = unreal.EditorAssetLibrary
    levels = unreal.EditorLevelLibrary
    if assets.does_asset_exist(MAP_PATH):
        existing = assets.load_asset(MAP_PATH)
        if not isinstance(existing, unreal.World):
            raise TypeError("目标目录已有非地图资产：" + MAP_PATH)
        if MODE != "calibrate":
            print("FOLIAGE_REVIEW_MAP_ALREADY_EXISTS", MAP_PATH)
            return

        # 仅对本脚本生成的开发审核地图设置预热，不改变平台Niagara或正式前端工作室。
        unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
        actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        matches = [a for a in actor_api.get_all_level_actors()
                   if a.get_actor_label() == "FoliageReview_PeachNiagara"]
        if len(matches) != 1:
            raise RuntimeError("审核地图预期恰好存在一个专属桃花NiagaraActor")
        component = matches[0].get_component_by_class(unreal.NiagaraComponent)
        if component is None or component.get_asset() is None:
            raise RuntimeError("审核地图NiagaraComponent尚未有效绑定")
        if component.get_asset().get_path_name().split(".")[0] != SYSTEM_PATH:
            raise RuntimeError("当前审核Actor引用了意外Niagara系统，拒绝修改")
        # GPU发射器启动的第一张离屏截图可能先于首批粒子产生；预热仅为可视审核。
        component.set_editor_property("override_warmup_settings", True)
        component.set_editor_property("warmup_tick_count", 150)
        component.set_editor_property("warmup_tick_delta", 1.0 / 30.0)
        if not levels.save_current_level():
            raise RuntimeError("无法保存审核关卡预热设置")
        print("FOLIAGE_REVIEW_MAP_WARMUP_SAVED", MAP_PATH, "5s")
        return

    system = assets.load_asset(SYSTEM_PATH)
    ground = assets.load_asset(GROUND_MESH)
    if not isinstance(system, unreal.NiagaraSystem):
        raise FileNotFoundError("缺失正式桃花Niagara资产：" + SYSTEM_PATH)
    if not isinstance(ground, unreal.StaticMesh):
        raise FileNotFoundError("缺失引擎地面模型：" + GROUND_MESH)
    niagara_cls = getattr(unreal, "NiagaraActor", None)
    if niagara_cls is None:
        raise RuntimeError("UE5.8没有NiagaraActor Python反射类型，禁止生成空占位地图")

    folder = MAP_PATH.rsplit("/", 1)[0]
    if not assets.does_directory_exist(folder):
        assets.make_directory(folder)
    if not levels.new_level(MAP_PATH):
        raise RuntimeError("UE编辑器创建审核地图失败：" + MAP_PATH)

    def spawn(cls, name, pos, rot=(0.0, 0.0, 0.0)):
        item = levels.spawn_actor_from_class(
            cls, unreal.Vector(*pos), unreal.Rotator(*rot))
        if item is None:
            raise RuntimeError("审核地图Actor创建失败：" + name)
        item.set_actor_label(name)
        return item

    # 演示地面仅为视觉参考，不改变玩法碰撞。初始桃花地面高度为-1cm。
    floor = spawn(unreal.StaticMeshActor, "FoliageReview_Ground", (0, 0, -1.0))
    mesh = floor.get_editor_property("static_mesh_component")
    mesh.set_static_mesh(ground)
    mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    floor.set_actor_scale3d(unreal.Vector(22.0, 22.0, 1.0))

    # 运行时仅一个真实Niagara组件，外观变体由项目Definition/场景参数选择。
    foliage = spawn(niagara_cls, "FoliageReview_PeachNiagara", (0, 0, 0))
    component = foliage.get_component_by_class(unreal.NiagaraComponent)
    if component is None:
        raise RuntimeError("NiagaraActor未生成合法UNiagaraComponent")
    component.set_asset(system)
    component.set_editor_property("auto_activate", True)
    # 仅开发审核Actor使用，首帧即可观察已生成粒子；正式前端不继承该预热。
    component.set_editor_property("override_warmup_settings", True)
    component.set_editor_property("warmup_tick_count", 150)
    component.set_editor_property("warmup_tick_delta", 1.0 / 30.0)

    for cls, name, pos in (
        (unreal.DirectionalLight, "FoliageReview_Sun", (50, 0, 850)),
        (unreal.SkyLight, "FoliageReview_Sky", (0, 0, 700)),
        (unreal.SkyAtmosphere, "FoliageReview_Atmosphere", (0, 0, 0)),
    ):
        spawn(cls, name, pos)

    # 编辑器可用相机；PIE还可用PlayerStart快速观察。
    spawn(unreal.CameraActor, "FoliageReview_Camera",
          (-1000, 0, 250), (-10, 0, 0))
    spawn(unreal.PlayerStart, "FoliageReview_PlayerStart",
          (-950, 0, 200), (-9, 0, 0))

    if not levels.save_current_level():
        raise RuntimeError("保存真实落叶审核地图失败")
    saved = assets.load_asset(MAP_PATH)
    if not isinstance(saved, unreal.World):
        raise RuntimeError("审核地图保存后重新读取失败")
    print("FOLIAGE_REVIEW_UE_MAP_SAVED", MAP_PATH)
    print("FOLIAGE_REVIEW_ACTOR", foliage.get_name(),
          "SYSTEM", component.get_asset().get_path_name())


if __name__ == "__main__":
    execute()
