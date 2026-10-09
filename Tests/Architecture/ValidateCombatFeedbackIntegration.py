#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》三层命中反馈与已授权技能预热静态门禁。

范围仅限文本源码/插件描述/构建规则，不代替UE UHT编译、自动化实跑、
资产注册表、Client/Server Cook或联机/输入手感验收。
"""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
issues: list[str] = []
checked = 0


def source(relative_path: str) -> str:
    """读取仓库内真实文件；缺失一律失败，不以空字符串冒充已完成。"""
    target = ROOT / relative_path
    if not target.is_file():
        issues.append(f"文件缺失：{relative_path}")
        return ""
    return target.read_text(encoding="utf-8-sig")


def check(condition: bool, requirement: str) -> None:
    """逐项计数，失败时提供中文原因供C++工程实施者定位。"""
    global checked
    checked += 1
    if not condition:
        issues.append(requirement)


def plugin(relative_path: str) -> dict:
    text = source(relative_path)
    try:
        return json.loads(text)
    except (ValueError, TypeError) as exc:
        issues.append(f"插件JSON描述非法：{relative_path}: {exc}")
        return {}


arena_plugin = plugin("Game/Plugins/DivineBeasts/DBAArena/DBAArena.uplugin")
common_plugin = plugin("Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin")
platform_plugin = plugin(
    "Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/"
    "GamePlatformPresentation.uplugin"
)
moba_plugin = plugin(
    "Game/Plugins/MobaCommon/Presentation/MobaPresentation/MobaPresentation.uplugin"
)

def dependency_names(descriptor: dict) -> set[str]:
    return {entry.get("Name", "") for entry in descriptor.get("Plugins", [])}

check(
    {"DBAGameplay", "DBAClient", "MobaPresentation",
     "GamePlatformCombat", "GamePlatformData", "GamePlatformPresentation"}
    .issubset(dependency_names(arena_plugin)),
    "竞技项目层缺少Gameplay/DBAClient/Moba/Combat/Data/Presentation插件的直接声明",
)
check(
    "MobaPresentation" not in dependency_names(common_plugin)
    and "DBAArena" not in dependency_names(common_plugin),
    "公共DBAClient被错误引入可选竞技插件依赖",
)
check(
    "GamePlatformData" in dependency_names(platform_plugin),
    "平台Presentation Profile未声明GamePlatformData依赖",
)
check(
    "DivineBeasts" not in str(moba_plugin.get("Plugins", [])),
    "MOBA通用插件向项目层建立了反向依赖",
)
check(
    any(m.get("Name") == "DivineBeastsArenaClient"
        and m.get("Type") == "ClientOnly"
        for m in arena_plugin.get("Modules", [])),
    "项目竞技客户端组合根不在ClientOnly模块",
)
check(
    "DivineBeastsAbilitiesRuntime" in source(
        "Game/Plugins/DivineBeasts/DBAArena/Source/"
        "DivineBeastsArenaClient/DivineBeastsArenaClient.Build.cs"),
    "竞技组合根缺少对现有项目技能授权Runtime的构建依赖",
)

profile = source(
    "Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/"
    "Source/GamePlatformPresentationCore/Public/Feedback/GamePlatformHitFeedbackProfile.h"
)
catalog_h = source(
    "Game/Plugins/DivineBeasts/DBAClient/Source/"
    "DivineBeastsPresentationRuntime/Public/Definitions/DivineBeastsCombatFeedbackCatalog.h"
)
catalog_cpp = source(
    "Game/Plugins/DivineBeasts/DBAClient/Source/"
    "DivineBeastsPresentationRuntime/Private/Definitions/DivineBeastsCombatFeedbackCatalog.cpp"
)
client_h = source(
    "Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/"
    "Private/Feedback/DivineBeastsArenaCombatFeedbackClientSubsystem.h"
)
client_cpp = source(
    "Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/"
    "Private/Feedback/DivineBeastsArenaCombatFeedbackClientSubsystem.cpp"
)
moba_cpp = source(
    "Game/Plugins/MobaCommon/Presentation/MobaPresentation/"
    "Source/MobaPresentationClient/Private/MobaPresentationClientSubsystem.cpp"
)
hitstop_cpp = source(
    "Game/Plugins/GamePlatform/Gameplay/GamePlatformAnimation/"
    "Source/GamePlatformAnimationClient/Private/Feedback/GamePlatformLocalHitstopSubsystem.cpp"
)

check(
    "UGamePlatformHitFeedbackProfile : public UGamePlatformDefinitionBase" in profile,
    "通用Profile不使用唯一GamePlatformDefinition主资产体系",
)
check(
    profile.count('AssetBundles="Client"') == 2,
    "CameraShake与Overlay必须只预加载到客户端Client Bundle",
)
check(
    "FPrimaryAssetId ProfileDefinitionId;" in catalog_h
    and "TSoftObjectPtr<UGamePlatformHitFeedbackProfile>" not in catalog_h,
    "项目反馈目录未使用平台主资产ID，会绕过正式Data租约",
)
check(
    "ValidateMappings" in catalog_cpp and "ValidateDefinition" in catalog_cpp
    and "SeenKeys.Contains" in catalog_cpp and "ProfileDefinitionId.PrimaryAssetType" in catalog_cpp,
    "目录未提供可复核的唯一键/主资产发布验证契约",
)
check(
    "PlayerControllerChanged" in client_h
    and "HandlePossessedPawnChanged" in client_h
    and "OnPossessedPawnChanged.AddUniqueDynamic" in client_cpp,
    "本地玩家Pawn切换时未按生命周期订阅真实技能授权",
)
check(
    "GameStateSetEvent.AddUObject" in client_cpp
    and "GameStateSetEvent.Remove" in client_cpp,
    "GameState晚到/跨图事件未绑定和撤销，竞技Profile可能错过初始化或泄漏",
)
check(
    "OnLoadoutChanged().AddUObject" in client_cpp
    and "OnLoadoutChanged().Remove" in client_cpp,
    "技能授权快照监听器未建立配对的生命周期清理",
)
check(
    "State.bReady" in client_cpp
    and "State.HeroDefinitionId" in client_cpp
    and "State.AvatarGeneration" in client_cpp
    and "Slot.AbilityId" in client_cpp
    and "Slot.AbilityLevel" in client_cpp,
    "反馈预热未严格限制为服务器已授予的当前英雄实际技能",
)
check(
    "PrewarmAuthorizedLocalAbilities" in client_cpp
    and "for (const FDivineBeastsCombatFeedbackEntry& Entry : LoadedCatalog->Entries)"
    not in client_cpp,
    "目录加载完成后仍无差别预热十二生肖全部反馈资源",
)
check(
    "GetGameState<AGamePlatformArenaGameState>" in client_cpp
    and "EGamePlatformDataLifetime::Instance" in client_cpp
    and "CancelWorldLeases()" in client_cpp,
    "非竞技世界资源隔离或实例租约显式释放边界缺失",
)
check(
    "GetLoadedDefinition(" in client_cpp
    and "LoadSynchronous(" not in client_cpp
    and "LoadObject(" not in client_cpp,
    "命中或技能授权热路径进行了同步资源加载",
)
check(
    "bProjectFallback" in moba_cpp
    and "NAME_None : HitVFXDefinitionId" in moba_cpp
    and "NAME_None : HitSFXDefinitionId" in moba_cpp,
    "无匹配技能Profile时可能错误复用前一位英雄的VFX/SFX",
)
check(
    "IsPlayingRootMotion()" in hitstop_cpp
    and "Mesh->bPauseAnims" in hitstop_cpp
    and "FPlatformTime::Seconds()" in hitstop_cpp
    and "SetGlobalTimeDilation" not in hitstop_cpp,
    "局部动画顿帧缺少根运动保守跳过/独立时钟或错误冻结世界",
)
check(
    "gp.Combat.HitstopOverrideFrames" in hitstop_cpp
    and "ResolveVisualHitstopFrames" in hitstop_cpp
    and "GetValueOnGameThread" in hitstop_cpp,
    "客户端缺少0/3/6帧无服务器副作用的独立顿帧调试入口",
)
tuning_test = source(
    "Game/Plugins/GamePlatform/Gameplay/GamePlatformAnimation/"
    "Source/GamePlatformAnimationClient/Private/Tests/GamePlatformLocalHitstopTuningTests.cpp"
)
check(
    all(fragment in tuning_test for fragment in (
        'Frames(6, 0)', 'Frames(6, 3)', 'Frames(3, 6)', 'Frames(24, -1)')),
    "局部顿帧调试计算缺少0/3/6与上限测试用例",
)
check(
    not re.search(r'\.h"[ \t]*#include', client_cpp),
    "竞技组合根存在同一行拼接的#include预处理器错误",
)

if issues:
    print(f"三层命中反馈静态门禁：{checked}项，失败{len(issues)}项")
    for issue in issues:
        print(f"  - {issue}")
    raise SystemExit(1)

print(f"三层命中反馈静态门禁：{checked}项全部通过。")
print("注意：不代表UE编译/反射类装载/真实.uasset/Cook/联机或打击手感验收通过。")
