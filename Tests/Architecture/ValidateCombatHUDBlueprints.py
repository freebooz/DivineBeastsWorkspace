#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》战斗HUD控件蓝图及事件适配源码静态门禁。

严格区分：只检查Monolith已登记的实际引擎资产存在性、父类/命名绑定、C++事件接线合同；
不解析或更改.uasset，不把该脚本冒充Monolith回读/蓝图编译、UE5.8链接、双客户端测试。
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core"
CLIENT = ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient"
COMBAT = ROOT / "Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat"
MANIFEST = CONTENT / "Docs/MonolithGenerationManifest.json"
issues = []

if not MANIFEST.is_file():
    issues.append("缺少Monolith用户界面真实资产生成/修改记录")
    section = {}
else:
    doc = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    section = doc.get("LatestCombatHUDWidgetDelivery", {})
    if section.get("Tool") != "Monolith MCP" or section.get("Version") != "0.23.0":
        issues.append("现行战斗HUD必须由真实Monolith编辑器版本提供资产证据")
    if section.get("MonolithPackagesSaved") != 5:
        issues.append("最近战斗HUD要求五个实际资产均由引擎保存")
    if not section.get("SingleEditorAssetTreeReadback"):
        issues.append("Monolith须已回读实际蓝图控件树")
    if section.get("PIEAndDedicatedServerVerified"):
        issues.append("静态资产台账不得伪造实际PIE/联机验收状态")

expected = {
    "UI/Combat/WBP_DBA_UI_CombatHUD": (7, "DivineBeastsCombatPanelBase"),
    "UI/Root/WBP_DBA_UI_RootLayout": (11, "DivineBeastsRootLayout"),
    "UI/Components/WBP_DBA_UI_PlayerStatus": (7, "DivineBeastsPlayerStatusPanel"),
    "UI/Components/WBP_DBA_UI_TargetFrame": (6, "GamePlatformTargetFrameWidget"),
    "UI/Components/WBP_DBA_UI_StatusEffects": (11, "GamePlatformStatusEffectTrayWidget"),
}
assets = {entry["AssetPath"]: entry for entry in section.get("Assets", []) if "AssetPath" in entry}

for relative, (count, parent) in expected.items():
    package = "/DBAUIPack_Core/" + relative
    binary = CONTENT / "Content" / (relative + ".uasset")
    entry = assets.get(package)
    if not binary.is_file() or binary.stat().st_size <= 1024:
        issues.append("引擎实际二进制蓝图缺失或异常：" + str(binary))
    if not entry:
        issues.append("Monolith生成台账缺少目标控件资产：" + package)
        continue
    if entry.get("WidgetCount") != count:
        issues.append("控件树数量与本次Monolith回读证据不一致：" + package)
    if entry.get("ParentClass") != parent:
        issues.append("控件父类必须遵循三层公共→项目派生：" + package)
    if not entry.get("Saved") or entry.get("FreshCompileErrors") != 0 or entry.get("FreshCompileWarnings") != 0:
        issues.append("当前蓝图必须已编译零错误/警告并保存：" + package)

root_widget = assets.get("/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout", {})
if "HUDLayer/CombatHUD" not in root_widget.get("Contains", []):
    issues.append("战斗HUD必须真实组合进现有本地玩家RootLayout的HUDLayer")
if root_widget.get("DefaultCombatHUDVisibility") != "Collapsed":
    issues.append("登录/选角期间不得出现尚未授权的战斗HUD")
main_widget = assets.get("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD", {})
required_children = {"PlayerStatus", "PlayerPortrait", "TargetFrame", "StatusEffects", "AbilityBar", "CastBar"}
if not required_children.issubset(set(main_widget.get("Children", []))):
    issues.append("战斗HUD应复用现成的生命/技能/增减益等项目蓝图")
for name in ("UI/Components/WBP_DBA_UI_PlayerStatus", "UI/Components/WBP_DBA_UI_TargetFrame"):
    record = assets.get("/DBAUIPack_Core/" + name, {})
    if not any("Shield" in removed for removed in record.get("Removed", [])):
        issues.append("移除永久盾数值后必须清理项目层旧护盾条：" + name)

layout_h = (CLIENT / "Public/Layers/DivineBeastsRootLayout.h").read_text(encoding="utf-8-sig")
layout_cpp = (CLIENT / "Private/Layers/DivineBeastsRootLayout.cpp").read_text(encoding="utf-8-sig")
status_cpp = (CLIENT / "Private/Panels/Combat/DivineBeastsPlayerStatusPanel.cpp").read_text(encoding="utf-8-sig")
for token in ("BindWidgetOptional", "TObjectPtr<UDivineBeastsCombatPanelBase> CombatHUD"):
    if token not in layout_h:
        issues.append("RootLayout缺少战斗蓝图可选绑定合同：" + token)
for token in ("OnPossessedPawnChanged.AddUniqueDynamic", "OnPossessedPawnChanged.RemoveDynamic",
              "CombatHUD->SetVisibility", "BindCombatEffects(", "UnbindCombatEffects()",
              "HandleCombatTagChanged", "HandleCombatAttributeChanged",
              "GetDisplayGroupsView", "StatusEffects", "BuffItems", "DebuffItems", "CriticalItems"):
    if token not in layout_cpp:
        issues.append("HUD未实际接入Pawn/GAS事件或状态效果分组：" + token)
for token in ("RefreshStatusSourceFromOwningPawn", "BindToAbilitySystem(ASC)",
              "UnbindFromAbilitySystem()", "OnPossessedPawnChanged.RemoveDynamic"):
    if token not in status_cpp:
        issues.append("生命/气势面板缺少角色切换时事件重绑/清理：" + token)
if "Tick(" in layout_cpp or "Tick(" in status_cpp:
    issues.append("战斗HUD禁止逐帧轮询角色Gameplay状态")
vitals = (COMBAT / "Public/Attributes/GamePlatformCombatAttributeSet.h").read_text(encoding="utf-8-sig")
for retired in ("FGameplayAttributeData Shield;", "FGameplayAttributeData MaxShield;"):
    if retired in vitals:
        issues.append("旧Shield/MaxShield不能作为GAS永久字段复活")

report = {
    "status": "PASS" if not issues else "FAIL",
    "monolithAssetRecords": len(assets),
    "expectedReusedOrCreatedAssets": len(expected),
    "issues": issues,
    "limits": "仅静态结构门禁；Monolith当前编辑器编译/保存另有真实证据，C++模块与UE独立重载、PIE、双客户端、Cook需另测。",
}
print(json.dumps(report, indent=2, ensure_ascii=False))
sys.exit(0 if not issues else 1)
