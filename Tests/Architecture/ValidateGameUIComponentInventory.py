# -*- coding: utf-8 -*-
"""《神兽联盟》游戏通用UI组件、十大领域与蓝图交付状态只读门禁。

源文件与声明式规格属于工程代码/设计资产；只有Monolith实际生成的.uasset才算真实蓝图。
本脚本仅扫描，不创建任何.uasset、不会修改生产清单、不会替代UE UHT/C++或PIE/Cook验证。
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Saved/Monolith/GameUIComponentSpecs/Manifest.json"

PLUGINS = {
    "GamePlatformUIClient": ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/Public",
    "DivineBeastsUIClient": ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public",
    "DivineBeastsArenaClient": ROOT / "Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Public",
}
CONTENT_DIRS = {
    "/DBAUIPack_Core/UI": ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/UI",
    "/DBAArena/UI": ROOT / "Game/Plugins/DivineBeasts/DBAArena/Content/UI",
}

issues = []
if not MANIFEST.is_file():
    issues.append("未找到Monolith声明式规格清单，请先执行GenerateGameUIComponentSpecs.py")
    data = {"assets": [], "existingPreserve": []}
else:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))

items = data.get("assets", [])
if data.get("countToGenerate") != len(items):
    issues.append("声明式规格数量与索引计数不一致")
if len(items) != 32:
    issues.append(f"当前期望32份待制作规格，实际{len(items)}份")

parent_cache = {}
for module, directory in PLUGINS.items():
    # 当前入口模块真实公开类须可检索；文字检查只作为基本来源审计，不代表UHT通过。
    parent_cache[module] = "\n".join(
        p.read_text(encoding="utf-8-sig", errors="replace")
        for p in directory.rglob("*.h") if p.is_file())

delivered = []
pending = []
seen_assets = set()
domains = set()
for item in items:
    spec_path = ROOT / item.get("spec", "")
    mount_path = item.get("mountPath", "")
    if mount_path in seen_assets:
        issues.append(f"重复的蓝图挂载身份：{mount_path}")
    seen_assets.add(mount_path)
    domains.add(item.get("businessDomain", ""))

    if not spec_path.is_file():
        issues.append(f"规格不存在：{spec_path}")
        continue
    spec = json.loads(spec_path.read_text(encoding="utf-8"))
    if (spec.get("name") != item.get("assetName") or
        spec.get("parentClass") != item.get("parentClass") or
        spec.get("rootWidget", {}).get("type") != "CanvasPanel"):
        issues.append(f"规格与索引/根控件不一致：{item.get('id')}")

    parent = item.get("parentClass", "")
    try:
        module, class_name = parent.removeprefix("/Script/").split(".", 1)
        if module not in PLUGINS or f"U{class_name}" not in parent_cache[module]:
            issues.append(f"父类源头不存在：{parent}")
    except ValueError:
        issues.append(f"非法父类反射路径：{parent}")

    for mount, content in CONTENT_DIRS.items():
        if mount_path.startswith(mount+"/"):
            asset_path = content / (mount_path[len(mount)+1:] + ".uasset")
            break
    else:
        issues.append(f"蓝图根目录不符合三层所有权：{mount_path}")
        continue

    if asset_path.is_file() and asset_path.stat().st_size > 0:
        delivered.append(mount_path)
    else:
        pending.append(mount_path)

preserved = data.get("existingPreserve", [])
preserved_found = 0
for old in preserved:
    mount_path = old.get("path", "")
    for mount, content in CONTENT_DIRS.items():
        if mount_path.startswith(mount+"/"):
            asset_path = content / (mount_path[len(mount)+1:] + ".uasset")
            if asset_path.is_file() and asset_path.stat().st_size > 0:
                preserved_found += 1
            else:
                issues.append(f"历史已经交付的蓝图缺失：{asset_path}")
            break

# 三层边界：正式Server.Target不能启用纯UI，而Editor.Target需要同时启用公共和竞技蓝图拥有者。
target_dir = ROOT / "Game/Source"
editor_target = (target_dir / "DivineBeastsArenaEditor.Target.cs").read_text(encoding="utf-8")
server_target = (target_dir / "DivineBeastsArenaServer.Target.cs").read_text(encoding="utf-8")
if 'EnablePlugins.Add("DBAUIPack_Core")' not in editor_target:
    issues.append("编辑器目标未启用公共项目UI内容包")
if 'EnablePlugins.Add("DBAArena")' not in editor_target:
    issues.append("编辑器目标未启用竞技Widget父类插件")
if 'EnablePlugins.Add("DBAUIPack_Core")' in server_target:
    issues.append("专用服务器不应启用纯UI内容包")

result = {
    "check": "DivineBeasts game UI base classes, specs and actual assets",
    "expectedSpecs": len(items),
    "verifiedExistingWidgetFiles": preserved_found,
    "newWidgetBlueprintFilesFound": len(delivered),
    "widgetBlueprintsPending": len(pending),
    "domains": sorted(domains),
    "issues": issues,
    "staticCheckPassed": not bool(issues),
    "allVisualAssetsDelivered": len(pending) == 0 and not bool(issues),
    "meaning": "静态清单通过不代表真实蓝图已生产或编译。未交付的蓝图必须由Monolith MCP创建。",
}
print(json.dumps(result, ensure_ascii=False, indent=2))
raise SystemExit(1 if issues else 0)
