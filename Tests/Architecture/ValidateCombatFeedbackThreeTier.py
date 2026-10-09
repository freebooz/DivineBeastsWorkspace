#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
战斗打击反馈三层源码只读门禁。

本脚本只检查正式插件的最小目录、依赖方向、局部时钟与表现服务的静态约束；
不创建任何文件、不加载引擎、不修改资产或运行战斗。
返回0表示静态基线成立，不表示真实UE编译、Blueprint资产、Cook或多人联机成功。
"""
from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLUGINS = ROOT / "Game" / "Plugins"
RESULTS: list[tuple[str, bool]] = []


def check(label: str, condition: bool) -> None:
    RESULTS.append((label, condition))


def source(relative_path: str) -> str:
    """相对Workspace路径，强制存在UTF-8人工维护源码。"""
    return (ROOT / relative_path).read_text(encoding="utf-8-sig")


platform_combat = (
    "Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat/"
)
platform_animation = (
    "Game/Plugins/GamePlatform/Gameplay/GamePlatformAnimation/Source/GamePlatformAnimationClient/"
)
platform_camera = (
    "Game/Plugins/GamePlatform/Presentation/GamePlatformCamera/Source/GamePlatformCameraClient/"
)
moba_runtime = (
    "Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationRuntime/"
)
moba_client = (
    "Game/Plugins/MobaCommon/Presentation/MobaPresentation/Source/MobaPresentationClient/"
)
project_runtime = (
    "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/"
)

must_exist = {
    "P1 平台命中反馈Profile契约":
        "Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationCore/Public/Feedback/GamePlatformHitFeedbackProfile.h",
    "P2 视觉局部顿帧": platform_animation + "Private/Feedback/GamePlatformLocalHitstopSubsystem.cpp",
    "P2 动作输入缓冲":
        "Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Buffer/GamePlatformActionInputBuffer.cpp",
    "P3 受击Overlay闪白": platform_animation + "Private/Feedback/GamePlatformHitFlashWorldSubsystem.cpp",
    "P3 本地镜头Shake": platform_camera + "Private/Feedback/GamePlatformCameraHitFeedbackSubsystem.cpp",
    "P4 MOBA反馈强度": moba_runtime + "Private/Feedback/MobaHitFeedbackPolicy.cpp",
    "P5 神兽联盟技能映射": project_runtime + "Private/Definitions/DivineBeastsCombatFeedbackCatalog.cpp",
    "P6 已确认战斗事实网络契约": platform_combat + "Public/Types/GamePlatformCombatFeedbackNetEvent.h",
    "P6 客户端World总线": platform_combat + "Private/Subsystems/GamePlatformCombatFeedbackWorldSubsystem.cpp",
    "P7 战斗网络契约测试": platform_combat + "Private/Tests/GamePlatformCombatFeedbackNetTests.cpp",
    "P7 输入缓冲测试":
        "Game/Plugins/GamePlatform/Application/GamePlatformInput/Source/GamePlatformInputClient/Private/Tests/GamePlatformActionInputBufferTests.cpp",
    "P7 MOBA反馈策略测试": moba_runtime + "Private/Tests/MobaHitFeedbackPolicyTests.cpp",
}
for title, path in must_exist.items():
    check(title, (ROOT / path).is_file())

combat_src = source(platform_combat + "Private/Components/GamePlatformCombatComponent.cpp")
anim_src = source(platform_animation + "Private/Feedback/GamePlatformLocalHitstopSubsystem.cpp")
moba_src = source(moba_client + "Private/MobaPresentationClientSubsystem.cpp")
check("权威Combat保持服务器单向NetMulticast反馈", "MulticastConfirmedCombatFeedback(" in combat_src)
check("反馈网络不允许客户端ApplyDamage", "ApplyDamage(" not in source(platform_combat + "Private/Subsystems/GamePlatformCombatFeedbackWorldSubsystem.cpp"))
check("视觉顿帧采用单调实时时钟", "FPlatformTime::Seconds()" in anim_src)
check("视觉顿帧不停止全局时间", "SetGlobalTimeDilation" not in anim_src and "SetPause(" not in anim_src)
check("视觉顿帧使用正确的Ticker专属句柄",
      "FTSTicker::FDelegateHandle ActiveTickHandle" in
      source(platform_animation + "Public/Feedback/GamePlatformLocalHitstopSubsystem.h"))
check("MOBA复用现有Presentation提供者",
      all(x in moba_src for x in ('TEXT("VFX")', 'TEXT("SFX")', 'Presentation->Submit(Request)')))
check("MOBA调用局部闪白与摄像机而非自建播放器",
      "UGamePlatformHitFlashWorldSubsystem" in moba_src
      and "UGamePlatformCameraHitFeedbackSubsystem" in moba_src
      and "SpawnSystemAtLocation" not in moba_src)

for name, path in (
    ("DBAClient", "Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin"),
    ("MobaPresentation", "Game/Plugins/MobaCommon/Presentation/MobaPresentation/MobaPresentation.uplugin"),
):
    info = json.loads(source(path))
    deps = {v["Name"] for v in info.get("Plugins", [])}
    check(f"{name} 声明客户端动画服务依赖", "GamePlatformAnimation" in deps)
    if name == "MobaPresentation":
        check("MOBA摄像机插件显式Client/Editor目标隔离",
              any(v.get("Name") == "GamePlatformCamera"
                  and v.get("TargetAllowList") == ["Client", "Editor"]
                  for v in info.get("Plugins", [])))
    else:
        check("DBAClient SFX插件显式Client/Editor目标隔离",
              any(v.get("Name") == "GamePlatformSFX"
                  and v.get("TargetAllowList") == ["Client", "Editor"]
                  for v in info.get("Plugins", [])))

check("底层模块代码不硬引用生肖项目",
      "DivineBeasts" not in anim_src
      and "DivineBeasts" not in source(platform_camera + "Private/Feedback/GamePlatformCameraHitFeedbackSubsystem.cpp"))
check("保持项目模式注册目录在项目侧",
      "HeroDefinitionId" in source(project_runtime + "Public/Definitions/DivineBeastsCombatFeedbackCatalog.h"))

passed = sum(1 for _, good in RESULTS if good)
for label, good in RESULTS:
    print(("通过" if good else "失败") + "：" + label)
print(f"静态门禁：{passed}/{len(RESULTS)}；非引擎运行验收")
raise SystemExit(0 if passed == len(RESULTS) else 1)
