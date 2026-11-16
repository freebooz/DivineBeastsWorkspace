#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3地形GPU黑帧隔离试验：真实UE5.8 Slate多帧RenderTarget捕获。

此脚本由普通UnrealEditor.exe以-ExecCmds=py启动，独立打开十层V3审核地图。
使用SceneCapture2D连续帧捕获120帧后再读取RenderTarget（而非同步Capture后
立即导出），检查异步渲染时序是否是旧GPU截图黑帧的原因。
不保存临时Actor、地图、材质、PCG；实际画面是否非黑须另做像素测试。
禁止NullRHI，输出与旧失败截图分目录以便比较。
"""
from __future__ import annotations

import math
from pathlib import Path

import unreal  # type: ignore

ROOT = Path(__file__).resolve().parents[3]
MAP = "/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"
OUT = ROOT / "Game/Saved/Validation/TerrainV3DelayedSceneCaptureCameraFixed_20261018"
PNG = OUT / "TerrainV3_DelayedRenderTarget_D3D12_1280x720.png"
FRAME = 0
PREPARED = False
EXPORTED = False
FINISHED = False
HANDLE = None
ACTOR = None
TARGET = None


def done(reason: str) -> None:
    global FINISHED, HANDLE, ACTOR, TARGET
    if FINISHED:
        return
    FINISHED = True
    if HANDLE is not None:
        unreal.unregister_slate_post_tick_callback(HANDLE)
        HANDLE = None
    if ACTOR is not None:
        try:
            unreal.EditorLevelLibrary.destroy_actor(ACTOR)
        except Exception as exc:
            print("SSLM_V3_DELAYED_CAPTURE_CLEANUP_WARNING", repr(exc), flush=True)
        ACTOR = None
    TARGET = None
    print(
        "SSLM_V3_DELAYED_CAPTURE_TERMINAL",
        "reason=" + reason,
        "file_exists=" + str(PNG.is_file()).lower(),
        "actual_pixel_validation_required=true",
        flush=True,
    )
    unreal.SystemLibrary.quit_editor()


def tick(_delta: float) -> None:
    global FRAME, PREPARED, EXPORTED, ACTOR, TARGET
    if FINISHED:
        return
    FRAME += 1
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or MAP not in world.get_path_name():
        if FRAME > 500:
            done("qa_world_not_loaded")
        return

    if not PREPARED and FRAME >= 25:
        # 与前次重入修复一致：回调可能嵌套，先建立唯一状态后操作。
        PREPARED = True
        terrain = [
            a for a in unreal.EditorLevelLibrary.get_all_level_actors()
            if isinstance(a, unreal.LandscapeProxy)
        ]
        if len(terrain) != 1:
            done("landscape_actor_count_wrong")
            return
        origin, extent = terrain[0].get_actor_bounds(
            only_colliding_components=False
        )
        d = max(extent.x, extent.y)
        position = unreal.Vector(
            origin.x + d * 0.34,
            origin.y - d * 0.51,
            origin.z + d * 0.68 + max(6500.0, extent.z),
        )
        delta = unreal.Vector(
            origin.x - position.x,
            origin.y - position.y,
            origin.z - position.z,
        )
        # 严禁误用UE Python Rotator三个位置参数（roll,pitch,yaw）。
        look=unreal.Vector(origin.x,origin.y,origin.z)
        rotation=unreal.MathLibrary.find_look_at_rotation(position,look)
        forward=unreal.MathLibrary.get_forward_vector(rotation)
        mag=math.sqrt(delta.x**2+delta.y**2+delta.z**2)
        alignment=(forward.x*delta.x+forward.y*delta.y+forward.z*delta.z)/mag
        if alignment < .999:
            done("camera_forward_alignment_invalid")
            return
        print("SSLM_V3_DELAYED_CAPTURE_CAMERA_ALIGNMENT",round(alignment,6),flush=True)
        ACTOR = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D, position, rotation
        )
        if not isinstance(ACTOR, unreal.SceneCapture2D):
            done("scene_capture_actor_creation_failed")
            return
        TARGET = unreal.RenderingLibrary.create_render_target2d(
            world,
            1280,
            720,
            unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0, 0, 0, 1),
            False,
            False,
        )
        if not isinstance(TARGET, unreal.TextureRenderTarget2D):
            done("render_target_creation_failed")
            return
        comp = ACTOR.get_editor_property("capture_component2d")
        comp.set_editor_property("texture_target", TARGET)
        comp.set_editor_property("fov_angle", 65.0)
        comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property("capture_every_frame", True)
        comp.set_editor_property("capture_on_movement", False)
        post = unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method", True)
        post.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias", True)
        post.set_editor_property("auto_exposure_bias", 0.0)
        post.set_editor_property(
            "override_auto_exposure_apply_physical_camera_exposure", True
        )
        post.set_editor_property(
            "auto_exposure_apply_physical_camera_exposure", False
        )
        comp.set_editor_property("post_process_settings", post)
        comp.set_editor_property("post_process_blend_weight", 1.0)
        print(
            "SSLM_V3_DELAYED_CAPTURE_READY",
            "frame=" + str(FRAME),
            "landscape_bounds_cm=" + str(round(d, 1)),
            flush=True,
        )

    if PREPARED and not EXPORTED and FRAME >= 180 and TARGET is not None:
        EXPORTED = True
        # 这里已至少连续渲染155个Slate帧；此时从RenderTarget读取已有内容。
        try:
            unreal.RenderingLibrary.export_render_target(
                world, TARGET, str(OUT), PNG.name
            )
        except Exception as exc:
            print("SSLM_V3_DELAYED_CAPTURE_EXPORT_ERROR", repr(exc), flush=True)
            done("render_target_export_exception")
            return
        print(
            "SSLM_V3_DELAYED_CAPTURE_FILE_REQUESTED",
            "frame=" + str(FRAME),
            str(PNG),
            flush=True,
        )

    if EXPORTED and FRAME >= 190:
        if PNG.is_file():
            print(
                "SSLM_V3_DELAYED_CAPTURE_FILE_OBSERVED",
                "size=" + str(PNG.stat().st_size),
                flush=True,
            )
            done("file_written_needs_actual_pixel_qa")
            return
        if FRAME >= 300:
            done("render_target_output_missing")
            return

    if FRAME >= 600:
        done("timed_out")


if "-nullrhi" in unreal.SystemLibrary.get_command_line().lower():
    raise RuntimeError("NullRHI无真实渲染，禁止用空图伪装GPU审核")
if PNG.exists():
    raise FileExistsError("已有本轮独立截图，绝不覆盖：" + str(PNG))
OUT.mkdir(parents=True, exist_ok=True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
print(
    "SSLM_V3_DELAYED_CAPTURE_SCHEDULED",
    "map=" + MAP,
    "no_qa_map_save=true",
    "gpu_pixel_qa_pending=true",
    flush=True,
)
