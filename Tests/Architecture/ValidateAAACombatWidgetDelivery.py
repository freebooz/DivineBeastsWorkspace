# -*- coding: utf-8 -*-
"""《神兽联盟》战斗P0实际Widget蓝图交付台账静态门禁。

职责与边界：
- 只读检查新战斗UI内容包.uasset文件、父类/路径登记、历史资源保全和最新文档引用；
- 不加载/编辑虚幻二进制包、不自行生成.uasset、不代替Monolith编译或PIE/Cook；
- 运行位置固定为DivineBeastsWorkspace（神兽联盟工作空间），不影响其他并行项目。

运行：python -X utf8 Tests/Architecture/ValidateAAACombatWidgetDelivery.py
预期：17个非空真实资产、12项P0覆盖、6个旧Widget仍存在、无重复资产身份；
否则以非零退出码阻止把失真的交付台账宣称为完成。
"""
import json
from collections import Counter
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[2]
CONTENT = WORKSPACE / (
    "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
    "DBAUIPack_Core/Content/UI"
)
DOCS = CONTENT.parents[1] / "Docs"
MANIFEST_PATH = DOCS / "AAACombatWidgetDelivery_20261009.json"
HISTORY_PATH = DOCS / "MonolithGenerationManifest.json"

EXPECTED = {
    "CastBar", "TargetFrame", "CombatAlert", "BuffTray", "DebuffTray",
    "ControlAlert", "StatusEffectIcon", "BossCastAlert", "TargetDebuffTray",
    "DispelBadge", "ImmunityBadge", "EffectOverflow",
    "HealthBar", "PlayerPortrait", "AbilitySlot", "PlayerStatus", "StatusEffects",
}
EXPECTED_OLD = {
    "Root/WBP_DBA_UI_RootLayout.uasset",
    "Screens/WBP_DBA_UI_Login.uasset",
    "Screens/WBP_DBA_UI_CharacterCreate.uasset",
    "Screens/WBP_DBA_UI_CharacterSelect.uasset",
    "Components/WBP_DBA_HeroChoice.uasset",
    "Components/WBP_DBA_CharacterChoice.uasset",
}

issues = []
if not MANIFEST_PATH.is_file() or not HISTORY_PATH.is_file():
    raise SystemExit("缺失新资产台账或旧MonolithGenerationManifest（历史资产核验清单）")

manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8-sig"))
historical = json.loads(HISTORY_PATH.read_text(encoding="utf-8-sig"))
rows = manifest.get("assets", [])
name_counts = Counter()

for row in rows:
    path = row.get("AssetPath", "")
    name = path.rsplit("/", 1)[-1].removeprefix("WBP_DBA_UI_")
    name_counts[name] += 1
    if not path.startswith("/DBAUIPack_Core/UI/Components/"):
        issues.append("资产归属没有限定在第三层项目UI内容包：" + path)
        continue
    if row.get("ParentClass", "").startswith("/Script/"):
        module = row["ParentClass"].split("/Script/", 1)[1].split(".", 1)[0]
        if module not in {"GamePlatformUIClient", "DivineBeastsUIClient"}:
            issues.append("异常跨层Widget父类：" + path)
    else:
        issues.append("蓝图原生父类缺少/Script反射路径：" + path)
    native = CONTENT / "Components" / (path.rsplit("/", 1)[-1] + ".uasset")
    if not native.is_file() or native.stat().st_size == 0:
        issues.append("登记资产不存在或为空：" + path)
    if row.get("BuilderResponse", {}).get("bSuccess") is not True:
        issues.append("未保存成功的Monolith制作响应：" + path)
    if row.get("BuilderResponse", {}).get("error_count") != 0:
        issues.append("Monolith编译错误未归零：" + path)
    if row.get("BuilderResponse", {}).get("warning_count") != 0:
        issues.append("Monolith编译警告未归零：" + path)

if set(name_counts) != EXPECTED:
    issues.append("17项P0及通用组件清单不完整：" + str(sorted(set(EXPECTED) - set(name_counts))))
if any(count != 1 for count in name_counts.values()):
    issues.append("同一Widget蓝图标识重复登记")
if manifest.get("createdAssetCount") != 17 or len(rows) != 17:
    issues.append("实际资源登记数与17项交付约定不符")
if manifest.get("combatP0SpecsSatisfied") != 12:
    issues.append("战斗P0专项的12项交付统计不准确")

old_rows = historical.get("Assets", [])
old_paths = {row.get("AssetPath") for row in old_rows}
if len(old_rows) != 6:
    issues.append("原有6份历史Widget被改变数量")
for rel in EXPECTED_OLD:
    native = CONTENT / rel
    if not native.is_file() or native.stat().st_size == 0:
        issues.append("旧Widget遗失：" + rel)
    mount_path = "/DBAUIPack_Core/UI/" + rel.removesuffix(".uasset")
    if mount_path not in old_paths:
        issues.append("旧Widget从历史Manifest移除：" + mount_path)

latest = historical.get("LatestAAACombatWidgetDelivery", {})
if latest.get("RealWidgetBlueprintCount") != 17:
    issues.append("历史Manifest未引用本轮17份真实新资产")
if latest.get("AllAssetsIndependentlyRestartReadback") is True:
    issues.append("尚未完成独立编辑器重启，不得宣告全资产重启回读通过")
if latest.get("PIEAndCookPassed") is True:
    issues.append("尚未执行PIE/Cook，不得宣告发布验证通过")
if not (DOCS / "AAACombatWidgetDelivery_20261009.md").is_file():
    issues.append("缺少对应的中文交付说明文档")

result = {
    "result": "PASS" if not issues else "FAIL",
    "contentPack": "DBAUIPack_Core（神兽联盟项目UI内容包）",
    "newBlueprintAssetsVerified": len(rows),
    "combatP0Blueprints": manifest.get("combatP0SpecsSatisfied"),
    "historicWidgetBlueprintsPreserved": len(old_rows),
    "compileAndSaveAuthority": "Monolith MCP 0.23.0（仅源自实际制作响应）",
    "independentEditorRestartVerified": False,
    "PIEAndCookVerified": False,
    "issues": issues,
}
print(json.dumps(result, ensure_ascii=False, indent=2))
raise SystemExit(0 if not issues else 1)
