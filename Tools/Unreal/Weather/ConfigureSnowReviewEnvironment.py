#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""仅在正式UE5.8编辑器中配置独立积雪审核地图的初始大雪演示。

文件用途：给人工审查与PIE自动化提供明确可见的天气测试初值。
归属：Tools/Unreal/Weather（编辑器美术创作工具），只修改Development/Snow的审核地图。
安全：默认inspect只读；apply只允许更改本地图唯一SnowReview_WeatherAuthority Actor的审核属性。
不修改正式新手村地图，不修改服务器业务天气定义，不覆盖其他场景或UI资产。
异常或无唯一Actor直接失败，不产生未受控的资源移动或伪uasset。
"""
from __future__ import annotations
import os

MODE = os.getenv("SNOW_REVIEW_CONFIG_MODE", "inspect").strip().lower()
MAP = "/Game/Development/Snow/Maps/L_DBA_SnowReview"
ACTOR_LABEL = "SnowReview_WeatherAuthority"

def main() -> None:
    if MODE not in ("inspect", "apply"):
        raise ValueError("SNOW_REVIEW_CONFIG_MODE只允许inspect/apply")
    import unreal  # type: ignore
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝在非正式DivineBeastsArena编辑器修改地图：" + project)
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
        raise FileNotFoundError("真实积雪审核地图不存在：" + MAP)
    if MODE == "inspect":
        print("SNOW_REVIEW_CONFIG_INSPECT_ONLY", MAP)
        return
    # 用当前独立编辑器进程加载已生成的开发地图；这里不删除/移动资产。
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if editor is None or not editor.load_level(MAP):
        raise RuntimeError("UE未能加载独立积雪审核地图")
    matches = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
               if a.get_actor_label() == ACTOR_LABEL]
    if len(matches) != 1:
        raise RuntimeError("审核Actor数量异常，应严格为1：" + str(len(matches)))
    actor = matches[0]
    if not isinstance(actor, unreal.DivineBeastsWeatherReviewController):
        raise TypeError("审核Actor父类不匹配，拒绝修改：" + actor.get_class().get_name())
    snow_type = unreal.GamePlatformWeatherType.HEAVY_SNOW
    # 单独的审核地图需要打开时可明确进入大雪，不应由真实关卡人员临时猜测状态。
    actor.set_editor_property("apply_at_begin_play", True)
    actor.set_editor_property("preview_weather", snow_type)
    actor.set_editor_property("preview_transition_seconds", 0.0)
    if not editor.save_current_level():
        raise RuntimeError("UE未能保存审核地图，不能声明人工入口已配置")
    if (not actor.get_editor_property("apply_at_begin_play") or
        actor.get_editor_property("preview_weather") != snow_type or
        actor.get_editor_property("preview_transition_seconds") != 0.0):
        raise RuntimeError("审核Actor属性设置与回读不一致")
    print("SNOW_REVIEW_CONFIG_SAVED", MAP, ACTOR_LABEL, "HeavySnow", "transition=0s")
    print("SNOW_REVIEW_NEXT_GATE:真实PIE可见雪面和Niagara/声音，人工审核仍需独立完成")

if __name__ == "__main__":
    main()
