# -*- coding: utf-8 -*-
"""3A游戏用户界面十大业务域研究台账静态门禁。

仅验证资料整理的完整性、ID唯一、分期分类与已声明的历史蓝图是否真实存在。
不能证明263个组件已经实现，也不能代替UE/Monolith资产校验。
"""
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "Game/Plugins/DivineBeasts/DBAClient/Docs"
MANIFEST = DOCS / "AAAUIComponentInventoryV1.json"
CATALOG = DOCS / "3A游戏UI十大业务域组件总清单_V1.0.md"
STATUS_SPEC = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Docs/AAA状态效果UI设计规范_V1.0.md"
DOMAINS = {
    "Core", "Account", "World", "Character", "Inventory",
    "Combat", "Social", "Arena", "System", "LiveOps",
}
PHASES = {"P0", "P1", "P2"}
LAYERS = {
    "GamePlatform（通用平台）", "MobaCommon（MOBA通用）",
    "DivineBeasts（项目层）",
}
issues = []
if not MANIFEST.is_file():
    raise SystemExit("没有发现AAA界面机器台账")

data = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
items = data.get("componentRecords", [])
ids = set()
for ix, item in enumerate(items):
    component_id = item.get("componentId", "")
    domain = item.get("domain", "")
    if component_id in ids:
        issues.append(f"重复ID：{component_id}")
    ids.add(component_id)
    if domain not in DOMAINS or not component_id.startswith(domain + "."):
        issues.append(f"领域与唯一ID不一致：{component_id}")
    if item.get("priority") not in PHASES:
        issues.append(f"优先级错误：{component_id}")
    if item.get("layer") not in LAYERS:
        issues.append(f"三层归属错误：{component_id}")
    if not item.get("zhName") or not item.get("description"):
        issues.append(f"缺少中文职责：{component_id}")
    # 蓝图资源已存在标记必须经过实际磁盘检测；不可只根据JSON属性宣告交付。
    original_path = item.get("blueprintPath")
    if original_path:
        if original_path.startswith("/DBAUIPack_Core/UI/"):
            suffix = original_path.removeprefix("/DBAUIPack_Core/UI/") + ".uasset"
            real_file = (
                ROOT /
                "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
                "DBAUIPack_Core/Content/UI" /
                suffix
            )
            if not real_file.is_file() or real_file.stat().st_size == 0:
                issues.append(f"台账中旧蓝图被标注为存在但磁盘缺失：{component_id}")
        else:
            issues.append(f"当前已存在资产挂载点未识别：{component_id}")

by_domain = Counter(item["domain"] for item in items)
by_phase = Counter(item["priority"] for item in items)
if set(by_domain) != DOMAINS or data.get("domainCount") != 10:
    issues.append("十大业务域分类不全")
if data.get("componentCount") != len(items):
    issues.append("总组件数与机器台账实际条数不一致")
if not CATALOG.is_file() or not STATUS_SPEC.is_file():
    issues.append("缺少中文组件总清单或Buff/Debuff状态效果设计文档")
else:
    overview = CATALOG.read_text(encoding="utf-8-sig")
    for item in items:
        if ("`" + item["componentId"] + "`") not in overview:
            issues.append(f"机器台账与中文清单缺项：{item['componentId']}")
    for token in ["FGamePlatformUIStatusEffect", "EffectInstanceId", "DispelCategory"]:
        if token not in STATUS_SPEC.read_text(encoding="utf-8-sig"):
            issues.append(f"状态效果设计字段缺失：{token}")

result = {
    "status": "PASS" if not issues else "FAIL",
    "documentType": "目标组件研究清单，不是实现或蓝图交付证明",
    "domains": {domain: by_domain[domain] for domain in sorted(DOMAINS)},
    "count": len(items),
    "phases": dict(sorted(by_phase.items())),
    "issues": issues[:30],
}
print(json.dumps(result, ensure_ascii=False, indent=2))
raise SystemExit(0 if not issues else 1)
