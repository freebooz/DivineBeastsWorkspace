#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3地形截图任务生命周期回归（纯Python，不需要UE或GPU）。

此测试只能验证脚本可重入性/资源回收/明确失败门禁，不能替代真实
UE5.8 D3D12像素、Shader/GPU性能、Client/Server Cook或人工画面签收。
"""
from __future__ import annotations

import ast
import math
from pathlib import Path
from types import SimpleNamespace
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUTHOR = ROOT / "Tools/Unreal/Terrain/CaptureTerrainV3TickedViewportUE58.py"
MAP = "/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"


class LandscapeProxy:
    """模拟只读Landscape包围盒。"""

    def get_actor_bounds(self, only_colliding_components=False):
        assert not only_colliding_components
        return (
            SimpleNamespace(x=0.0, y=0.0, z=6000.0),
            SimpleNamespace(x=50000.0, y=50000.0, z=9000.0),
        )


class CameraActor:
    """模拟临时审核摄像机。"""


class FakePNG:
    """模拟尚未完成的异步截图，防止测试冒充真实GPU文件。"""

    def is_file(self):
        return False


class FakeTask:
    """UE AutomationEditorTask（异步截图任务）的最小测试替身。"""

    def is_valid_task(self):
        return True

    def is_task_done(self):
        return True


class Engine:
    def __init__(self):
        self.calls = 0
        self.exits = 0
        self.camera_destroyed = 0
        self.camera_created = 0
        self.callbacks_unregistered = 0
        self.tick = None
        world = SimpleNamespace(get_path_name=lambda: MAP + "." + MAP.rsplit("/", 1)[-1])

        def reenter_on_screenshot(*args, **kwargs):
            self.calls += 1
            assert len(args) == 3
            assert kwargs.get("camera") is not None
            assert kwargs.get("force_game_view") is True
            # 模拟截图API递归派发Slate Tick：旧作者会重复提交截图。
            for _ in range(4):
                self.tick(1.0 / 60.0)
            return FakeTask()

        self.unreal = SimpleNamespace(
            LandscapeProxy=LandscapeProxy,
            CameraActor=CameraActor,
            EditorLevelLibrary=SimpleNamespace(
                get_editor_world=lambda: world,
                get_all_level_actors=lambda: [LandscapeProxy()],
                spawn_actor_from_class=self.spawn_camera,
                set_level_viewport_camera_info=lambda *_: None,
                destroy_actor=self.destroy_camera,
            ),
            AutomationLibrary=SimpleNamespace(
                get_editor_active_viewport_view_mode=lambda: "VMI_LIT",
                finish_loading_before_screenshot=self.reenter_during_loading,
                take_high_res_screenshot=reenter_on_screenshot,
            ),
            Vector=lambda x, y, z: SimpleNamespace(x=x, y=y, z=z),
            Rotator=lambda pitch, yaw, roll: (pitch, yaw, roll),
            # 审查作者已纠正错误的Rotator位置参数，改用UE原生FindLookAtRotation。
            # 模拟ForwardVector和真正的目标方向对齐，验证>=0.999门禁。
            MathLibrary=SimpleNamespace(
                find_look_at_rotation=lambda eye, target: (eye, target),
                get_forward_vector=lambda rot: SimpleNamespace(
                    x=(rot[1].x-rot[0].x)/math.dist(
                        (rot[1].x,rot[1].y,rot[1].z),
                        (rot[0].x,rot[0].y,rot[0].z)),
                    y=(rot[1].y-rot[0].y)/math.dist(
                        (rot[1].x,rot[1].y,rot[1].z),
                        (rot[0].x,rot[0].y,rot[0].z)),
                    z=(rot[1].z-rot[0].z)/math.dist(
                        (rot[1].x,rot[1].y,rot[1].z),
                        (rot[0].x,rot[0].y,rot[0].z)),
                ),
            ),
            SystemLibrary=SimpleNamespace(quit_editor=self.quit_editor),
            unregister_slate_post_tick_callback=self.unregister,
        )

    def spawn_camera(self, cls, _pos, _rot):
        self.camera_created += 1
        return cls()

    def reenter_during_loading(self):
        # 模拟UE5.8资源等待过程中同步触发Slate Tick，不能重复生成相机。
        for _ in range(3):
            self.tick(1.0 / 60.0)

    def destroy_camera(self, camera):
        assert isinstance(camera, CameraActor)
        self.camera_destroyed += 1

    def unregister(self, _handle):
        self.callbacks_unregistered += 1

    def quit_editor(self):
        self.exits += 1


class TerrainV3ScreenshotLifecycleTests(unittest.TestCase):
    def test_source_parses_and_has_official_async_task_contract(self):
        text = AUTHOR.read_text(encoding="utf-8")
        ast.parse(text, filename=str(AUTHOR))
        self.assertIn("SCREENSHOT_TASK.is_task_done()", text)
        self.assertIn("SCREENSHOT_TASK.is_valid_task()", text)
        self.assertIn("unreal.SystemLibrary.quit_editor()", text)
        self.assertIn("REQUESTED = True", text)
        self.assertIn("camera=CAMERA", text)
        self.assertIn("pixel_validation_required=true", text)
        self.assertNotIn('execute_console_command(world, "QUIT")', text)
        self.assertIn("no_active_rendering_editor_viewport", text)
        self.assertIn("screenshot_task_timeout", text)

    def test_nested_slate_callback_submits_exactly_one_screenshot(self):
        tree = ast.parse(AUTHOR.read_text(encoding="utf-8"))
        selected = [
            item for item in tree.body
            if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef))
            and item.name in ("tick", "stop")
        ]
        self.assertEqual({item.name for item in selected}, {"tick", "stop"})
        mock = Engine()
        ns = {
            "unreal": mock.unreal,
            "math": math,
            "MAP": MAP,
            "PNG": FakePNG(),
            "HANDLE": object(),
            "CAMERA": None,
            "SCREENSHOT_TASK": None,
            "FRAME": 0,
            "PREPARED": False,
            "REQUESTED": False,
            "FINISHED": False,
            "LAST_REPORT": 0,
        }
        fn = compile(ast.Module(body=selected, type_ignores=[]), str(AUTHOR), "exec")
        exec(fn, ns)
        mock.tick = ns["tick"]
        for _ in range(185):
            mock.tick(1.0 / 60.0)
        self.assertTrue(ns["REQUESTED"])
        self.assertTrue(ns["FINISHED"])
        self.assertEqual(mock.calls, 1, "截图API递归Tick不得重复提交")
        self.assertEqual(mock.exits, 1, "只关闭当前临时审核编辑器一次")
        self.assertEqual(mock.callbacks_unregistered, 1)
        self.assertEqual(mock.camera_destroyed, 1)
        self.assertEqual(mock.camera_created, 1, "等待资源加载期间的递归SlateTick不得重复创建摄像机")
        self.assertIsNone(ns["CAMERA"])


if __name__ == "__main__":
    unittest.main()
