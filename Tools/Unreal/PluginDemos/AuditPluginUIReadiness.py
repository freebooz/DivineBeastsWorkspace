# -*- coding: utf-8 -*-
"""SSLM 插件演示能力差距审计（只读，不能替人批准或伪造已完成）。

输入为版本化 PluginDemoManifest.json 的真实四态能力索引；67个插件菜单并非
67项可运行功能。本工具只计算审查工作单和各插件缺口，不修改编目真源或Widget。
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "Docs/Implementation/PluginDemos/PluginDemoManifest.json"


def summarize(data: dict) -> dict:
    """只信源编目，未知功能数量不以0代表插件不具备功能。"""
    features_by_plugin: dict[str, list] = defaultdict(list)
    scenarios_by_feature: dict[str, list] = defaultdict(list)
    for item in data.get("features", []):
        features_by_plugin[item["plugin_id"]].append(item)
    for item in data.get("scenarios", []):
        scenarios_by_feature[item["feature_id"]].append(item)

    records = []
    statuses = Counter()
    actual_feature_ids = {item["feature_id"] for item in data.get("features", [])}
    unknown_refs = []
    for item in data.get("scenarios", []):
        if item["feature_id"] not in actual_feature_ids:
            unknown_refs.append(item["scenario_id"])

    for plugin in data.get("plugins", []):
        identity = plugin["plugin_id"]
        state = plugin["feature_audit_status"]
        statuses[state] += 1
        feats = features_by_plugin.get(identity, [])
        scenario_count = sum(len(scenarios_by_feature.get(f["feature_id"], [])) for f in feats)
        # Schema2作者真源只允许not_reviewed；人工签收来自独立可审计运行证明，
        # 不能通过修改编目字段伪造审核通过。
        reviewed = 0
        # source-stated状态说明实际完成范围；功能缺项不能依据文件数量猜测为0。
        work = ("audit_real_features_and_owners" if state == "unreviewed"
                else "complete_unreviewed_features_and_native_runs")
        records.append({
            "pluginId": identity,
            "layer": plugin["architecture_layer"],
            "kind": plugin["plugin_kind"],
            "auditStatus": state,
            "registeredFeatures": len(feats),
            "wiredFeatures": sum(f["integration_status"] == "wired" for f in feats),
            "registeredScenarios": scenario_count,
            "reviewedFeatures": reviewed,
            "nextAction": work,
            "coveragePercent": None,  # 未逐插件完成能力分母，不得伪算覆盖率。
        })

    known = len(data.get("features", []))
    approved = 0  # 独立签收/运行报告尚未由本只读脚本加载和认证，作者文件无批准权。
    return {
        "source": "PluginDemoManifest.json (schema v2)",
        "pluginCount": len(records),
        "pluginStatusCounts": dict(sorted(statuses.items())),
        "registeredFeatureCount": known,
        "wiredFeatureCount": sum(p["wiredFeatures"] for p in records),
        "registeredScenarioCount": len(data.get("scenarios", [])),
        "reviewedFeatureCount": approved,
        "capabilityDenominatorKnown": False,
        "overallCoveragePercent": None,
        "productionDemoVerified": False,  # 真实Native/人工证明尚未被此作者级清单验证。
        "unknownScenarioFeatureRefs": unknown_refs,
        "approvalEvidenceSource": "requires_separate_native_and_human_review_reports",
        "plugins": records,
    }


def main():
    parser = argparse.ArgumentParser(description="SSLM 插件演示逐项差距审查")
    parser.add_argument("--summary", action="store_true", help="输出精简统计，省略逐插件对象")
    parser.add_argument("--require-ready", action="store_true",
                        help="显式发布门禁；仍有未审核/未运行的功能则返回非0")
    args = parser.parse_args()
    raw = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    report = summarize(raw)
    if args.summary:
        report.pop("plugins")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if args.require_ready and not report["productionDemoVerified"]:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
