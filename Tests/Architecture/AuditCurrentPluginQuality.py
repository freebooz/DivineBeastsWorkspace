#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM当前全部插件源码、模块、性能风险候选和人工演示覆盖盘点。

默认只读，--write-docs 才写派生台账。仅基于真实描述/文件/正式编目。
统计项不可替代UE原生构建、蓝图连线/运行、Insights或人工签收。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLUGINS = ROOT / "Game/Plugins"
DELIVERY = ROOT / "Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json"
DEMO = ROOT / "Docs/Implementation/PluginDemos/PluginDemoManifest.json"
OUTPUT = ROOT / "Docs/Implementation/PluginQualityAudit"

# 明确要求人工操作面板或3D演示的视觉、交互、世界插件；不是声称已有原生资产。
VISUAL_CODE = frozenset((
    "DBAClient DBAArena DBAGameplay DBAWorlds MobaPresentation GamePlatformArena "
    "GamePlatformAI GamePlatformAnimation GamePlatformCamera GamePlatformCharacter "
    "GamePlatformCombat GamePlatformCommerceUI GamePlatformEquipment GamePlatformGameplay "
    "GamePlatformInput GamePlatformInteraction GamePlatformInventory GamePlatformLoading "
    "GamePlatformLobby GamePlatformNavigation GamePlatformPCG GamePlatformPresentation "
    "GamePlatformProgression GamePlatformQuest GamePlatformSFX GamePlatformSettings "
    "GamePlatformSurface GamePlatformUI GamePlatformVFX GamePlatformVillage "
    "GamePlatformWater GamePlatformWeather GamePlatformWorld GamePlatformDebug"
).split())
PERFORMANCE = {
    "全世界Actor遍历": re.compile(r"\b(?:GetAllActorsOfClass|GetAllActorsWithTag|GetAllActorsWithInterface)\s*\("),
    "同步资源加载": re.compile(r"\b(?:LoadSynchronous|StaticLoadObject|LoadObject)\s*[<(]"),
    "逐帧执行": re.compile(r"\b(?:NativeTick|TickComponent|Tick)\s*\("),
    "Ticker轮询": re.compile(r"\bAddTicker\s*\("),
}


# 已经核对的真实UE开发诊断关卡快捷入口；仅供人工观察/操作起点，不是正式功能案例。
# 每个稳定插件身份最多关联一张代表性地图；共享地图不得重复计入功能完成。
DIAGNOSTIC_MAPS = {
    "GamePlatformPCG": "/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1",
    "GamePlatformWater": "/Game/Development/Water/Maps/L_GP_WaterReview",
    "GamePlatformWeather": "/Game/Development/Weather/Maps/L_DBA_WeatherReview",
    "GamePlatformCamera": "/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview",
    "GamePlatformCombat": "/Game/Development/Foundation/PluginReviews/Maps/L_GP_CombatReview",
    "GamePlatformSFX": "/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview",
    "GamePlatformInteraction": "/Game/Development/Foundation/PluginReviews/Maps/L_GP_InteractionAIReview",
    "GamePlatformAI": "/Game/Development/Foundation/PluginReviews/Maps/L_GP_InteractionAIReview",
    "GamePlatformSurface": "/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview",
    "GamePlatformWorld": "/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview",
    "DBAWorlds": "/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview",
    "GamePlatformAnimation": "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase",
    "GamePlatformCharacter": "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase",
    "DBAGameplay": "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase",
    "GamePlatformArena": "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_MainArenaReview",
    "DBAArena": "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_MainArenaReview",
    "DBAClient": "/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview",
    "GamePlatformVFX": "/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review/L_DBA_IceArrow_A_Review",
    "DBAPresentationPack_Core": "/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review/L_DBA_IceArrow_A_Review",
}


def physical_diagnostic_map(root: Path, package: str) -> Path:
    """仅两种受控挂载点映射，防止把任意/绝对路径伪装为已审核的原生资源。"""
    if package.startswith("/Game/Development/"):
        return root / "Game/Content" / (package.removeprefix("/Game/") + ".umap")
    if package.startswith("/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review/"):
        return (root / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
                "DBAPresentationPack_Core/Content" /
                (package.removeprefix("/DBAPresentationPack_Core/") + ".umap"))
    raise ValueError("未批准的诊断地图挂载点：" + package)


def valid_ue_map_header(path: Path) -> bool:
    if not path.is_file() or path.stat().st_size <= 64:
        return False
    with path.open("rb") as source:
        return source.read(4) == bytes.fromhex("c1 83 2a 9e")


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def layer_of(path: Path) -> str:
    name = path.relative_to(PLUGINS).parts[0]
    return {"GamePlatform": "Platform", "MobaCommon": "Moba"}.get(name, "Project")


def audit(root: Path, demo: dict, delivery: dict) -> dict:
    descriptors = sorted((root / "Game/Plugins").rglob("*.uplugin"))
    manifest = {p["plugin_id"]: p for p in demo["plugins"]}
    listed = {p["Id"]: p for p in delivery["Plugins"]}
    features = defaultdict(list)
    scenarios = defaultdict(list)
    for item in demo.get("features", []):
        features[item["plugin_id"]].append(item)
    for item in demo.get("scenarios", []):
        scenarios[item["feature_id"]].append(item)
    actual = {p.stem: p for p in descriptors}
    issues = []
    if len(actual) != len(descriptors):
        issues.append("物理插件描述存在重复身份")
    if set(actual) != set(manifest) or set(actual) != set(listed):
        issues.append("67份真实描述与演示/交付编目的身份集合不一致")
    results = []
    module_owners = {}
    edges = {}
    performance_count = Counter()
    # 真实可操作案例与仅供观察的UE开发地图必须分列，不可将后者计入正式覆盖率。
    for plugin_id, package in DIAGNOSTIC_MAPS.items():
        if plugin_id not in actual:
            issues.append("诊断地图快捷入口引用不存在的插件：" + plugin_id)
        else:
            file_path = physical_diagnostic_map(root, package)
            if not valid_ue_map_header(file_path):
                issues.append("诊断地图原生包缺失或签名异常：" + plugin_id + " → " + package)
    for path in descriptors:
        name = path.stem
        d = load_json(path)
        owner = path.parent
        layer = layer_of(path)
        modules = d.get("Modules", [])
        kind = "code" if modules else "content_pack"
        builds = []
        for mod in modules:
            module = mod.get("Name", "")
            build = owner / "Source" / module / (module + ".Build.cs")
            if not build.is_file():
                issues.append(name + "/" + module + " 缺少Build.cs")
            else:
                builds.append(build.relative_to(root).as_posix())
            if module in module_owners:
                issues.append("重复模块身份：" + module)
            module_owners[module] = name
        cpp = [p for p in owner.rglob("*.cpp") if not any(part in ("Intermediate","Binaries","Saved") for part in p.parts)]
        headers = [p for p in owner.rglob("*.h") if not any(part in ("Intermediate","Binaries","Saved") for part in p.parts)]
        tests = [p for p in cpp if "Tests" in p.parts]
        assets = [p for p in owner.rglob("*") if p.is_file() and p.suffix.lower() in (".uasset",".umap")
                  and not any(part in ("Intermediate","Binaries","Saved") for part in p.parts)]
        maps = [p for p in assets if p.suffix.lower() == ".umap"]
        widget_files = [p for p in assets if p.stem.startswith(("WBP_","WB_","BP_UI_"))]
        candidates = []
        for p in cpp:
            # 不把Editor-only模块或WITH_EDITOR编译路径误判为Client/Server运行时热点。
            # 此处仅对常见显式预处理边界做保守扫描，复杂宏仍需独立UE构建/性能工具确认。
            # 模块身份只能从本插件Source/<ModuleName>获得；
            # 不能误匹配上级插件根目录名（例如PCGEditor源码路径也含PCG插件根名）。
            parts = p.relative_to(owner).parts
            module = parts[1] if len(parts) >= 3 and parts[0] == "Source" else ""
            host = next((str(m.get("Type", "")) for m in modules
                         if m.get("Name") == module), "")
            if "Tests" in p.parts or host.startswith("Editor"):
                continue
            guards = []
            for line_no, line in enumerate(p.read_text(encoding="utf-8-sig",errors="replace").splitlines(),1):
                stripped = line.lstrip()
                if stripped.startswith(("#if ", "#ifdef ", "#ifndef ")):
                    guards.append(stripped.startswith(("#if WITH_EDITOR", "#if UE_EDITOR", "#ifdef WITH_EDITOR")))
                    continue
                if stripped.startswith("#else"):
                    if guards:
                        guards[-1] = not guards[-1]
                    continue
                if stripped.startswith("#elif"):
                    if guards:
                        guards[-1] = False
                    continue
                if stripped.startswith("#endif"):
                    if guards:
                        guards.pop()
                    continue
                if any(guards) or stripped.startswith(("//","*","/*","#")):
                    continue
                for category,pattern in PERFORMANCE.items():
                    if pattern.search(line):
                        performance_count[category] += 1
                        if sum(c["kind"] == category for c in candidates) < 2:
                            candidates.append({"kind":category,"file":p.relative_to(root).as_posix(),"line":line_no})
        deps = [m.get("Name","") for m in d.get("Plugins",[]) if isinstance(m,dict) and m.get("Enabled",True)]
        edges[name] = deps
        if len(deps) != len(set(deps)):
            issues.append(name + " 重复插件依赖")
        fts = features[name]
        cases = [s for f in fts for s in scenarios[f["feature_id"]]]
        need_visual = kind == "content_pack" or name in VISUAL_CODE or bool(widget_files)
        known_diagnostic = DIAGNOSTIC_MAPS.get(name)
        faults = [v for v in issues if v.startswith(name + "/") or v.startswith(name + " ")]
        if faults:
            priority = "P0"
        elif need_visual and not cases:
            priority = "P1"
        elif not tests and kind == "code" or candidates or manifest.get(name,{}).get("feature_audit_status") != "complete":
            priority = "P2"
        else:
            priority = "P3"
        results.append({
            "pluginId":name, "layer":layer, "kind":kind, "descriptor":path.relative_to(root).as_posix(),
            "moduleCount":len(modules), "moduleBuildRules":builds,
            "cppFiles":len(cpp), "headerFiles":len(headers), "nativeTestFiles":len(tests),
            "nativeAssets":len(assets), "mapAssets":len(maps), "widgetAssets":len(widget_files),
            "visualReviewRequired":need_visual,
            "registeredFeatures":len(fts), "wiredFeatures":sum(x.get("integration_status")=="wired" for x in fts),
            "registeredScenarios":len(cases),
            "diagnosticMapPackage": known_diagnostic,
            "diagnosticMapIsFormalScenario": False,
            "catalogStatus":manifest.get(name,{}).get("feature_audit_status","missing"),
            "humanApproved":False, "hasHumanOperationalScenario":bool(cases),
            "performanceReviewCandidates":candidates,
            "priority":priority,
            "workOrder":(
                "修复描述、编目及构建依赖" if faults else
                "制作真实UMG/地图+可启动/执行/停止/重置/返回的人工演示案例" if need_visual and not cases else
                "运行已登记案例、补齐功能分母及人工证据" if need_visual else
                "原生模块回归、接口/生命周期及候选热点复核"
            ),
        })
    ranks = {"Platform":0,"Moba":1,"Project":2}
    for source,deps in edges.items():
        for dep in deps:
            if dep in actual and ranks[layer_of(actual[dep])] > ranks[layer_of(actual[source])]:
                issues.append("跨层反向插件依赖："+source+" -> "+dep)
    results.sort(key=lambda r:r["pluginId"].casefold())
    return {
        "schemaVersion":1, "source":"actual uplugin/Build.cs/C++/uasset/umap plus authoritative delivery/demo manifests",
        "state":"STATIC_INVENTORY_ONLY_NOT_ACCEPTANCE",
        "limitations":"性能候选不是实测瓶颈；地图/蓝图文件不是UE加载证明；人工签收需要独立真实证据",
        "pluginCount":len(results), "code":sum(r["kind"]=="code" for r in results),
        "content":sum(r["kind"]=="content_pack" for r in results),
        "modules":sum(r["moduleCount"] for r in results),
        "withLocalNativeTests":sum(r["nativeTestFiles"]>0 for r in results),
        "visualRequired":sum(r["visualReviewRequired"] for r in results),
        "visualWithoutOperationalScenario":sum(r["visualReviewRequired"] and not r["registeredScenarios"] for r in results),
        "demoFeatures":len(demo.get("features",[])), "demoScenarios":len(demo.get("scenarios",[])),
        "humanApproved":0,
        "diagnosticShortcutPluginCount": sum(
            bool(r["diagnosticMapPackage"]) for r in results),
        "uniqueApprovedDiagnosticMapCount": len(set(DIAGNOSTIC_MAPS.values())),
        "priorityCounts":dict(Counter(r["priority"] for r in results)),
        "perfCandidates":dict(performance_count),
        "issues":issues, "plugins":results,
    }


def markdown(report: dict) -> str:
    lines = [
        "# SSLM 全量插件质量与可视化人工验收审查台账",
        "",
        "> 来源：实际工作树67个插件描述、源码、资源及正式双台账；静态盘点不替代UE编译、蓝图验收、Unreal Insights性能实测或人工签收。",
        "",
        "## 基线与处理顺序",
        "",
        f"- 代码插件{report['code']}、内容包{report['content']}、声明模块{report['modules']}；已登记演示功能{report['demoFeatures']}、场景{report['demoScenarios']}，人工签收0。",
        f"- 要求可视化/交互人工验证的插件{report['visualRequired']}，尚无登记操作案例的{report['visualWithoutOperationalScenario']}。功能总分母未逐项评审，不能计算真实覆盖率。",
        f"- 有本模块原生测试源码的插件{report['withLocalNativeTests']}。没有本地文件不等于项目不存在跨模块自动化测试。",
        f"- 已关联真实诊断图的插件{report['diagnosticShortcutPluginCount']}、不重复的诊断图{report['uniqueApprovedDiagnosticMapCount']}张；只是进入地图的观察入口，不计入正式演示案例和人工签收。",
        "",
        "| 插件 | 层级/类别 | 模块 | 源码(.cpp) | 原生资产/地图 | 测试源 | 要求人工界面 | 诊断图快捷入口 | 正式演示案例 | 编目审查 | 优先级 |",
        "|---|---|---:|---:|---:|---:|---|---|---:|---|---|",
    ]
    for p in report["plugins"]:
        fields = [p["pluginId"],p["layer"]+"/"+p["kind"],str(p["moduleCount"]),
                  str(p["cppFiles"]),str(p["nativeAssets"])+"/"+str(p["mapAssets"]),
                  str(p["nativeTestFiles"]),("是" if p["visualReviewRequired"] else "否"),
                  ("有（待人工检查）" if p["diagnosticMapPackage"] else "无"),
                  str(p["registeredScenarios"]),p["catalogStatus"],p["priority"]]
        lines.append("| " + " | ".join(fields) + " |")
    lines.extend([
        "", "## 审查与优化规范", "",
        "- P0：描述/模块/依赖存在明确错误时阻断编译；需结合已有模块闭包、继承与Client/Server Cook门禁进一步复核。",
        "- P1：可视化或交互插件没有登记操作案例时属于人工验收交付缺口。复用现有DemoHub与开发地图、Widget，不创建另一套入口；必须真实可进入、演示、操作、停止、重置、返回和生成待审证据。",
        "- P2：对Tick、Ticker、同步加载、全世界Actor搜索进行源码上下文与实际Insights测量，检查多世界隔离、取消/析构、限额和失败收口；文本匹配只属于候选。",
        "- P3：逐功能验证蓝图父类、依赖、可重入、输入、无障碍、PC/移动端、低中高画质、Dedicated Server资源隔离和多人实际运行。",
        "- 不可视化的纯服务允许以演示中心可操作诊断页面验收，不强制3D地图；尚无功能分母/原生证据前所有插件均不得标记生产完成。",
        "",
    ])
    if report["issues"]:
        lines.append("## 本次静态结构问题")
        lines.extend("- "+item for item in report["issues"])
    return "\n".join(lines)


def main() -> int:
    parser=argparse.ArgumentParser(description="SSLM全部插件静态质量及可视化验收门禁")
    parser.add_argument("--summary",action="store_true")
    parser.add_argument("--write-docs",action="store_true")
    parser.add_argument("--require-production",action="store_true")
    args=parser.parse_args()
    report=audit(ROOT,load_json(DEMO),load_json(DELIVERY))
    sys.path.insert(0,str(ROOT/"Tests/Architecture"))
    from ValidatePluginDeliveryManifest import validate
    report["issues"].extend(validate(ROOT,load_json(DELIVERY)))
    if args.write_docs and not report["issues"]:
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/"CurrentPluginQualityMatrix.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        (OUTPUT/"CurrentPluginQualityMatrix.md").write_text(markdown(report)+"\n",encoding="utf-8")
    view={k:v for k,v in report.items() if k!="plugins"} if args.summary else report
    print(json.dumps(view,ensure_ascii=False,indent=2))
    if report["issues"]:return 1
    if args.require_production:
        print("SSLM_PLUGIN_PRODUCTION_BLOCKED: 无67插件完整原生+人工性能签收",file=sys.stderr)
        return 2
    return 0


if __name__=="__main__":
    raise SystemExit(main())
