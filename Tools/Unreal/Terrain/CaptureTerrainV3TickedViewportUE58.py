#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3十层地形真实UE5.8编辑器视口截图（多帧、可重入安全）。

本脚本必须通过UnrealEditor.exe打开独立V3审核地图并使用-ExecCmds=py执行。
与-ExecutePythonScript的同步命令退出不同，使用Slate帧回调等待实际视口。
关键修复：
1. 截图API返回AutomationEditorTask，先设置REQUESTED再调用，防止在API
   触发嵌套SlateTick时重复提交截图，保留对象直到is_task_done()完成。
2. 显式临时CameraActor指定截图视角，避免使用其他打开视口的未知机位。
3. 检查活动Viewport（编辑器视口）可用性；无可用视口则严格失败。
4. 结束时使用SystemLibrary.quit_editor()（退出编辑器），不使用游戏QUIT命令。
5. PNG存在/截图Task结束不等于真实像素质量通过，须用独立像素门禁验收。
不修改/保存项目地图、PCG、Landscape、材质、专服资源或其他UE窗口。
失败证据原样保留，新截图目录独立于之前的黑帧试验。
"""
from __future__ import annotations

import math
from pathlib import Path

import unreal  # type: ignore

ROOT = Path(__file__).resolve().parents[3]
MAP = "/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"
OUT = ROOT / "Game/Saved/Validation/TerrainV3ViewportCameraFixed_20261018"
PNG = OUT / "TerrainV3_AfterRenderedFrames_1280x720.png"

FRAME = 0
PREPARED = False
REQUESTED = False
FINISHED = False
LAST_REPORT = 0
HANDLE = None
SCREENSHOT_TASK = None
CAMERA = None


def stop(reason: str) -> None:
    """确保一次性注销回调、销毁仅本任务生成的摄像机并退出本次编辑器。"""
    global HANDLE, CAMERA, FINISHED
    if FINISHED:
        return
    FINISHED = True
    if HANDLE is not None:
        unreal.unregister_slate_post_tick_callback(HANDLE)
        HANDLE = None
    if CAMERA is not None:
        try:
            unreal.EditorLevelLibrary.destroy_actor(CAMERA)
        except Exception as exc:
            print("SSLM_TERRAIN_V3_VIEWPORT_CAMERA_CLEANUP_WARNING", str(exc), flush=True)
        CAMERA = None
    print(
        "SSLM_TERRAIN_V3_VIEWPORT_TASK_TERMINAL",
        "reason=" + reason,
        "screenshot_exists=" + str(PNG.is_file()).lower(),
        "pixel_validation_required=true",
        flush=True,
    )
    # UE5.8官方PythonAPI：quit_editor退出编辑器，不是仅退出PIE/游戏世界。
    unreal.SystemLibrary.quit_editor()


def tick(_delta: float) -> None:
    """由真实Slate渲染帧推进，不阻塞主线程，也不以sleep假装RHI已经渲染。"""
    global FRAME, PREPARED, REQUESTED, LAST_REPORT, SCREENSHOT_TASK, CAMERA

    if FINISHED:
        return
    FRAME += 1
    if FRAME % 80 == 0 and FRAME != LAST_REPORT:
        LAST_REPORT = FRAME
        print("SSLM_TERRAIN_V3_VIEWPORT_TICK", FRAME, flush=True)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world or MAP not in world.get_path_name():
        if FRAME >= 480:
            stop("expected_qa_map_not_loaded")
        return

    if not PREPARED and FRAME >= 24:
        mode = unreal.AutomationLibrary.get_editor_active_viewport_view_mode()
        if mode is None or "unknown" in str(mode).lower():
            stop("no_active_rendering_editor_viewport")
            return

        targets = [
            actor
            for actor in unreal.EditorLevelLibrary.get_all_level_actors()
            if isinstance(actor, unreal.LandscapeProxy)
        ]
        if len(targets) != 1:
            stop("qa_landscape_count_not_one")
            return

        origin, extent = targets[0].get_actor_bounds(
            only_colliding_components=False
        )
        distance = max(extent.x, extent.y)
        if distance < 100.0:
            stop("qa_landscape_bounds_invalid")
            return
        position = unreal.Vector(
            origin.x + 0.35 * distance,
            origin.y - 0.53 * distance,
            origin.z + 0.85 * distance + extent.z,
        )
        dx = origin.x - position.x
        dy = origin.y - position.y
        dz = origin.z - position.z
        # Unreal Python Rotator的位置参数顺序与旧脚本假设不一致。
        # 使用引擎API产生实际LookAt，并在渲染前校验摄像机瞄准地面。
        target = unreal.Vector(origin.x, origin.y, origin.z)
        rotation = unreal.MathLibrary.find_look_at_rotation(position, target)
        forward = unreal.MathLibrary.get_forward_vector(rotation)
        distance_to_ground = math.sqrt(dx*dx + dy*dy + dz*dz)
        alignment=(forward.x*dx+forward.y*dy+forward.z*dz)/distance_to_ground
        print("SSLM_TERRAIN_V3_VIEWPORT_CAMERA_ALIGNMENT",round(alignment,6),flush=True)
        if alignment < 0.999:
            stop("camera_not_aimed_at_landscape")
            return

        # 临时CameraActor不给正式World保存任何地图更改。
        CAMERA = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.CameraActor, position, rotation
        )
        if not isinstance(CAMERA, unreal.CameraActor):
            stop("temporary_camera_not_created")
            return
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(
            position, rotation
        )
        # UE5.8的finish_loading_before_screenshot可能同步派发新的SlateTick。
        # 先标记已准备，保证临时CameraActor只生成一次；原先在等待之后设置会
        # 递归生成多名相机、污染本次审核World并导致截图视角竞争。
        PREPARED = True
        unreal.AutomationLibrary.finish_loading_before_screenshot()
        print(
            "SSLM_TERRAIN_V3_VIEWPORT_CAMERA_READY",
            "frame=" + str(FRAME),
            "view_mode=" + str(mode),
            "world=" + str(world.get_path_name()),
            flush=True,
        )

    if PREPARED and not REQUESTED and FRAME >= 155:
        # 截图函数内部可能派发嵌套Slate回调。必须先设置一次性提交标记，
        # 不允许函数未返回时重复启动多个高分辨率截图任务。
        REQUESTED = True
        try:
            SCREENSHOT_TASK = unreal.AutomationLibrary.take_high_res_screenshot(
                1280,
                720,
                str(PNG),
                camera=CAMERA,
                delay=0.35,
                force_game_view=True,
            )
            if (
                SCREENSHOT_TASK is None
                or not SCREENSHOT_TASK.is_valid_task()
            ):
                stop("screenshot_task_invalid")
                return
        except Exception as exc:
            print(
                "SSLM_TERRAIN_V3_VIEWPORT_SCREENSHOT_API_ERROR",
                repr(exc),
                flush=True,
            )
            stop("screenshot_api_error")
            return
        print(
            "SSLM_TERRAIN_V3_VIEWPORT_SCREENSHOT_SUBMITTED_ONCE",
            "frame=" + str(FRAME),
            str(PNG),
            flush=True,
        )

    if REQUESTED and SCREENSHOT_TASK is not None and FRAME >= 175:
        if SCREENSHOT_TASK.is_task_done():
            if PNG.is_file() and PNG.stat().st_size >= 30000:
                print(
                    "SSLM_TERRAIN_V3_VIEWPORT_TASK_OUTPUT_FILE",
                    "bytes=" + str(PNG.stat().st_size),
                    "pixel_qa_still_required=true",
                    flush=True,
                )
                stop("screenshot_task_done_file_present")
            else:
                stop("screenshot_task_done_file_missing_or_too_small")
            return

    if FRAME >= 480:
        stop("screenshot_task_timeout")


if PNG.exists():
    raise FileExistsError("本轮原生截图文件已存在，拒绝覆盖旧证据：" + str(PNG))
if not hasattr(unreal, "register_slate_post_tick_callback"):
    raise RuntimeError("UE5.8 Editor未提供Slate帧回调，禁止同步睡眠伪造多帧截图")
OUT.mkdir(parents=True, exist_ok=True)
HANDLE = unreal.register_slate_post_tick_callback(tick)
print(
    "SSLM_TERRAIN_V3_VIEWPORT_TASK_SCHEDULED",
    "map=" + MAP,
    "save_map=false",
    "need_manual_pixel_qa=true",
    flush=True,
)
