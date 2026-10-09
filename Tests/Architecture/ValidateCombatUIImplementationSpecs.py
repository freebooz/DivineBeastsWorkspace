# -*- coding: utf-8 -*-
"""战斗P0视觉蓝图规划/源码父类/真实资产的静态审计。

仅验证十二份Monolith规格和三项平台C++基础类的归属，
绝不把JSON存在、C++编译通过解释为Widget Blueprint已创建或保存。
需要先运行Tools/Unreal/UI/GenerateCombatUIImplementationSpecs.py。
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPECS = ROOT / "Saved/Monolith/CombatUIImplementationSpecs"
MANIFEST = SPECS / "Manifest.json"
PLATFORM_PUBLIC = ROOT / (
    "Game/Plugins/GamePlatform/Presentation/GamePlatformUI/"
    "Source/GamePlatformUIClient/Public/Components"
)
PLATFORM_PRIVATE = ROOT / (
    "Game/Plugins/GamePlatform/Presentation/GamePlatformUI/"
    "Source/GamePlatformUIClient/Private/Components"
)
PROJECT_ASSETS = ROOT / (
    "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
    "DBAUIPack_Core/Content/UI/Components"
)
BASELINE = ROOT / (
    "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
    "DBAUIPack_Core/Content/UI"
)

ISSUES = []
EXPECTED_NAMES = {
    "CastBar", "TargetFrame", "CombatAlert", "BuffTray", "DebuffTray",
    "ControlAlert", "StatusEffectIcon", "BossCastAlert", "TargetDebuffTray",
    "DispelBadge", "ImmunityBadge", "EffectOverflow",
}

if not MANIFEST.is_file():
    raise SystemExit("找不到Combat UI规格清单，先生成声明式JSON")

manifest = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
items = manifest.get("widgets", [])
if manifest.get("specCount") != 12 or len(items) != 12:
    ISSUES.append("状态/规格数量与十二项战斗视觉原子不一致")
if manifest.get("assetState") != "specification_only_not_compiled_or_saved":
    ISSUES.append("规格清单状态错误地声称了真实蓝图交付")

seen = set()
actual_assets = []
pending_assets = []
for item in items:
    name = item.get("name", "")
    asset_path = item.get("assetPath", "")
    parent = item.get("parentClass", "")
    relative_spec = item.get("spec", "")
    suffix = name.removeprefix("WBP_DBA_UI_")
    if suffix not in EXPECTED_NAMES:
        ISSUES.append("未知战斗UI视觉规范：" + name)
    if name in seen:
        ISSUES.append("重复的战斗蓝图资产命名：" + name)
    seen.add(name)
    if not asset_path.startswith("/DBAUIPack_Core/UI/Components/"):
        ISSUES.append("战斗蓝图没有归第三层公共UI内容包：" + name)
    if not parent.startswith("/Script/GamePlatformUIClient."):
        ISSUES.append("战斗视觉原子没有继承中立平台父类：" + name)
    else:
        cls_name = parent.split(".", 1)[1]
        if cls_name != "GamePlatformComponentWidget":
            head = PLATFORM_PUBLIC / (cls_name + ".h")
            src = PLATFORM_PRIVATE / (cls_name + ".cpp")
            if not head.is_file() or not src.is_file():
                ISSUES.append("平台父类缺少对应真实头文件/实现：" + cls_name)
    spec_file = ROOT / relative_spec
    if not spec_file.is_file():
        ISSUES.append("找不到Monolith布局JSON：" + relative_spec)
        continue
    spec = json.loads(spec_file.read_text(encoding="utf-8-sig"))
    if spec.get("name") != name or spec.get("parentClass") != parent:
        ISSUES.append("布局身份/父类与索引不一致：" + name)
    if spec.get("rootWidget", {}).get("type") != "CanvasPanel":
        ISSUES.append("布局根部应为CanvasPanel（画布面板）：" + name)
    if item.get("status") != "unverified_spec_only":
        ISSUES.append("布局规格错误申报完成：" + name)
    asset_file = PROJECT_ASSETS / (name + ".uasset")
    (actual_assets if asset_file.is_file() and asset_file.stat().st_size > 0
     else pending_assets).append(asset_path)

baseline_names = [
    "Root/WBP_DBA_UI_RootLayout.uasset",
    "Screens/WBP_DBA_UI_Login.uasset",
    "Screens/WBP_DBA_UI_CharacterCreate.uasset",
    "Screens/WBP_DBA_UI_CharacterSelect.uasset",
    "Components/WBP_DBA_HeroChoice.uasset",
    "Components/WBP_DBA_CharacterChoice.uasset",
]
baseline_found = 0
for name in baseline_names:
    f = BASELINE / name
    if f.is_file() and f.stat().st_size > 0:
        baseline_found += 1
    else:
        ISSUES.append("已交付Widget蓝图缺失：" + name)

result = {
    "staticValidation": "PASS" if not ISSUES else "FAIL",
    "declaredNewSpecs": len(items),
    "newRealWidgetAssets": len(actual_assets),
    "plannedWidgetsStillMissing": len(pending_assets),
    "existingWidgetAssetsPreserved": baseline_found,
    "issues": ISSUES,
    "warning": (
        "单次静态扫描不能证明Widget父类可加载或Monolith完成蓝图编译。"
        "真实新资产必须经编辑器编译保存回读。"
    ),
}
print(json.dumps(result, ensure_ascii=False, indent=2))
raise SystemExit(0 if not ISSUES else 1)
