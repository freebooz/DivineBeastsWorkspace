# -*- coding: utf-8 -*-
"""PCG金标准开发资产链静态合同回归（不依赖UEEditor，只校验创作一致性）。

本测试不生成资源、不运行UE、不修改源码；后续必须独立运行UE自动化和地图G01～G16。
"""
from __future__ import annotations

import ast
import re
import runpy
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[2]
TOOLS = WORKSPACE / "Tools/Unreal/PCG"
PCG = WORKSPACE / "Game/Plugins/GamePlatform/World/GamePlatformPCG"
GAME_CONFIG = WORKSPACE / "Game/Config/DefaultGame.ini"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def assert_equal(actual, expected, name: str) -> None:
    if actual != expected:
        raise AssertionError(f"{name}：实际={actual!r}，期望={expected!r}")



def check_blueprint_root_contracts(pipeline: str) -> None:
    """只读检查平台输入链与项目默认注入；真实规范化行为由Editor Automation覆盖。"""
    commands = PCG / "Source/GamePlatformPCGEditor/Private/Commands"
    authoring = text(commands / "GamePlatformPCGGoldMapAuthoring.cpp")
    header = text(commands / "GamePlatformPCGGoldMapAuthoring.h")
    command = text(commands / "GamePlatformPCGGoldAssetsCommandlet.cpp")
    for source in (PCG / "Source").rglob("*"):
        if source.suffix in {".cpp", ".h", ".cs"}:
            assert not re.search(r'"/DBA[A-Za-z0-9_]+/', text(source)), \
                f"平台PCG源码不可持有项目挂载点默认值：{source.relative_to(PCG)}"

    for name in ("Create", "Repair"):
        assert f"bool {name}(const FString& BlueprintRoot, FString& Error);" in header, \
            f"{name}必须显式接收蓝图根，不提供平台项目默认值"
        assert f"GamePlatformPCGGoldMap::{name}(BlueprintRoot, Error)" in command, \
            f"Commandlet必须传递已校验根到{name}"
    preflight = authoring.split("bool Preflight(", 1)[1].split("template<typename TActor>", 1)[0]
    assert "NormalizeBlueprintRoot(BlueprintRoot, NormalizedBlueprintRoot, Error)" in preflight, \
        "直接平台创作调用也必须校验输入，不能只依赖命令行调用者"
    assert preflight.index("NormalizeBlueprintRoot(") < preflight.index("TargetFilename"), \
        "蓝图根必须在资产检查及写入前完整校验"
    assert "*NormalizedBlueprintRoot, Item.Blueprint, Item.Blueprint" in preflight, \
        "真实ClassPath必须使用规范化后的显式根"
    normalizer = authoring.split("bool GamePlatformPCGGoldMap::NormalizeBlueprintRoot(", 1)[1].split(
        "bool GamePlatformPCGGoldMap::Create(", 1)[0]
    assert "TrimStartAndEnd()" in normalizer and "FPaths::NormalizeDirectoryName(Normalized)" in normalizer, \
        "目录规范化必须复用真实UE路径入口"
    assert 'FPackageName::IsValidLongPackageName(' in normalizer and \
        'Normalized + TEXT("/BP_PCG_Path"), false, &Reason' in normalizer, \
        "实际蓝图包名必须经UE已注册挂载点和合法包名校验，不能自制正则冒充引擎合同"

    stage_guard = command.split("const bool bRequiresBlueprintRoot =", 1)[1].split("FString BlueprintRoot;", 1)[0]
    assert 'Stage.Equals(TEXT("Map")' in stage_guard and 'Stage.Equals(TEXT("RepairMap")' in stage_guard
    assert not any(f'TEXT("{stage}")' in stage_guard for stage in
                   ("Definitions", "Realized", "Verify", "SpatialProbe", "GeneratePreview")), \
        "仅真正加载蓝图的Map/RepairMap阶段要求BlueprintRoot"
    assert command.index('FParse::Value(*Params, TEXT("BlueprintRoot="), BlueprintRootInput)') < \
        command.index("bool bSuccess = false"), "写入路由前必须解析并校验BlueprintRoot"
    assert "NormalizeBlueprintRoot(BlueprintRootInput, BlueprintRoot, Error)" in command
    for name in ("Verify", "ProbeSpatial", "PreviewGenerated"):
        assert f"GamePlatformPCGGoldMap::{name}(Error)" in command, \
            f"{name}不应引入无用的项目目录输入"

    default_root = re.search(r"(?m)^\$blueprintRoot = '([^']+)'", pipeline)
    assert default_root, "项目入口必须持有明确的蓝图根默认值"
    assert_equal(default_root.group(1), "/DBAWorldPack_Village/PCG/Blueprints/", "原Village蓝图根兼容")
    assert_equal(pipeline.count("('-BlueprintRoot=' + $blueprintRoot)"), 2,
                 "基础蓝图与Gold地图两个实际调用共用项目根")
    assert 'const FString GoldRoot = TEXT("/Game/Development/Foundation/PCG/");' in authoring
    assert 'const FString MapPackage = TEXT("/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1");' in authoring
    print("PCG_GOLD_BLUEPRINT_ROOT_STATIC_CONTRACTS_PASS")




def check_crop_spline_source_contract() -> None:
    """仅检查农田样条源迁移与平台安全边界；真实节点必须由UE5.8另行验收。"""
    generator = text(PCG / "Source/GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp")
    inspector = text(PCG / "Source/GamePlatformPCG/Private/Validation/PCGGraphInspection.cpp")
    approved = text(PCG / "Source/GamePlatformPCG/Private/Validation/GamePlatformPCGTemplateContract.cpp")
    editor_test = text(PCG / "Source/GamePlatformPCGEditor/Private/Tests/PCGEditorTests.cpp")
    script_path = TOOLS / "RepairGoldCropSplineInputUE58.py"
    script = text(script_path)
    runner = text(TOOLS / "RunGoldCropSplineRepairUE58.ps1")
    ast.parse(script, filename=str(script_path))

    for marker in (
        "UPCGDataFromActorSettings", "EPCGActorFilter::Self",
        "EPCGComponentSelection::ByClass", "USplineComponent::StaticClass()",
        "EPCGGetDataFromActorMode::ParseActorComponents",
        "PCGSplineSamplerConstants::SplineLabel",
    ):
        assert marker in generator, f"生成器缺少受限Actor样条源：{marker}"
    assert "UPCGDataFromActorSettings::StaticClass()" in approved, "PCG合同未批准官方安全查询节点"
    for marker in (
        "UnapprovedSplineActorSource", "CropSplineSourceNotConnected",
        "Source->ActorSelector.ActorFilter != EPCGActorFilter::Self",
        "Source->ComponentSelector.ComponentSelectionClass != USplineComponent::StaticClass()",
        "Source->bAlwaysRequeryActors", "SoleEdge(*SplineSourceNode",
    ):
        assert marker in inspector, f"PCG图审核缺少失败关闭规则：{marker}"
    for marker in (
        "EPCGActorFilter::AllWorldActors", "UBoxComponent::StaticClass()",
        "GamePlatformPCGInspection::ValidateApprovedGraph(*Profile)",
    ):
        assert marker in editor_test, f"Editor原生负向回归未覆盖：{marker}"
    assert 'PACKAGE = "/Game/Development/Foundation/PCG/Realized/PCG_Gold_Crop"' in script, \
        "迁移必须严格限制一份Gold Crop开发图"
    for marker in (
        "def inspect_graph(", "def assert_source(", "def require_enum(",
        'if MODE == "inspect":', 'if MODE == "verify":',
        'graph.remove_edge(', 'graph.add_node_of_type(', 'graph.add_edge(',
        "save_loaded_asset(", 'if sources:',
    ):
        assert marker in script, f"Gold Crop迁移步骤或幂等保护缺失：{marker}"
    for marker in (
        "Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool",
        "UnrealBuildTool", "Get-FileHash", "PCG_Gold_Crop.before.uasset",
        "SSLM_GOLD_CROP_SPLINE_REPAIR_PASS",
        "SSLM_GOLD_CROP_SPLINE_VERIFY_PASS",
        "LogPython: Error:", "Fatal error:",
    ):
        assert marker in runner, f"Gold Crop迁移安全/回执门禁缺失：{marker}"

    print("PCG_GOLD_CROP_SPLINE_STATIC_CONTRACT_PASS: source=SelfSpline, guard=fail-closed")
    recovery = text(TOOLS / "RunGoldCropRecoveryUE58.ps1")
    for marker in (
        "GoldCropSplineRepairUE58.ps1",  # 保持以下业务阶段引用真实受控工具，避免旁路
        "04-Build", "05-EditorAutomation", "06-GeneratePreview", "07-G09",
        "ReadyForApply", "BlockedByActiveEditorOrUBT",
        "GoldPreviewAndG09Passed_NotWholeGameProduction",
        "Get-Process UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool",
        "ProductionReady=$false",
    ):
        normalized_marker = "RunGoldCropSplineRepairUE58.ps1" if marker == "GoldCropSplineRepairUE58.ps1" else marker
        assert normalized_marker in recovery, f"Gold恢复编排缺少受控阶段或安全门禁：{normalized_marker}"
    print("PCG_GOLD_CROP_RECOVERY_PIPELINE_STATIC_PASS")


def check() -> None:
    for file in TOOLS.glob("*.py"):
        ast.parse(text(file), filename=str(file))
    defs = runpy.run_path(str(TOOLS / "AuthorGoldLevelDefinitions.py"))
    realized = runpy.run_path(str(TOOLS / "AuthorGoldRealizedGraphs.py"))
    map_script = runpy.run_path(str(TOOLS / "AuthorGoldLevelMap.py"))
    reopens = runpy.run_path(str(TOOLS / "ValidateGoldLevelAssets.py"))

    assert_equal(len(defs["ORDER"]), 17, "GoldDefinition数量")
    assert_equal(len(set(defs["ORDER"])), 17, "GoldDefinition名称唯一")
    assert_equal(len(defs["MESHES"]), 3, "Gold MeshSet数量")
    assert_equal(len(realized["DESCRIPTORS"]), 3, "金标准实际Spawner图数量")
    assert_equal(len(reopens["TEMPLATE_IDS"]), 12, "Foundation模板数量")
    assert_equal(len(reopens["SUBGRAPH_IDS"]), 7, "Foundation公共子图数量")
    assert_equal(len(reopens["BLUEPRINT_NAMES"]), 11, "PCG蓝图种类数")
    assert_equal(len(reopens["DEFINITIONS"]), 17, "重开校验Definition数量")
    assert_equal(
        {name for name, *_ in reopens["DEFINITIONS"]}, set(defs["ORDER"]),
        "创建脚本与独立重开定义清单一致"
    )
    assert_equal(
        {name for name, *_ in realized["DESCRIPTORS"]}, set(reopens["REALIZED_NAMES"]),
        "真正Spawner生成图与独立重开清单一致"
    )
    assert_equal(
        {name for _, name, *_ in map_script["SPECIFICATIONS"]},
        {
            "PCG_Canopy", "PCG_Rock", "PCG_Resource", "PCG_ManualLock",
            "PCG_MajorRoad", "PCG_MinorPath", "PCG_FieldFence",
            "PCG_Parcel", "PCG_Crops", "PCG_Gate", "PCG_HandPlacedBridge"
        },
        "金标准测试地图真实Actor清单"
    )
    assert set(bp for bp, *_ in map_script["SPECIFICATIONS"]).issubset(reopens["BLUEPRINT_NAMES"]), \
        "测试地图不能使用未生成或未验证的Blueprint（蓝图）"
    assert set(short for _, _, short, *_ in realized["DESCRIPTORS"]).issubset(defs["ORDER"]), \
        "实际图必须引用已登记的真实MeshSet Definition（网格集合定义）"
    assert all(based in reopens["TEMPLATE_IDS"] for _, based, *_ in realized["DESCRIPTORS"]), \
        "实际图模板必须属于12项Foundation Template"

    # C++ Editor生成器和独立Python清单名称必须一一对应，避免脚本引用不存在的蓝图资产。
    creator = text(PCG / "Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp")
    graph_writer = text(PCG / "Source/GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp")
    actor_code = text(PCG / "Source/GamePlatformPCG/Private/Actors/GamePlatformPCGActors.cpp")
    masks_header = text(PCG / "Source/GamePlatformPCG/Public/Services/GamePlatformPCGSpatialRules.h")
    masks_code = text(PCG / "Source/GamePlatformPCG/Private/Nodes/GamePlatformPCGSpatialRules.cpp")
    specs_block = creator.split("const TArray<FBlueprintSpec> Specifications =", 1)[1].split("};", 1)[0]
    blueprints = set(re.findall(r'TEXT\("(BP_PCG_[^"]+)"\)', specs_block))
    assert_equal(blueprints, set(reopens["BLUEPRINT_NAMES"]), "平台蓝图生成器与验证清单一致")
    for name in reopens["TEMPLATE_IDS"]:
        symbol = "FGamePlatformPCGTemplateIds::" + name.removeprefix("TPL_")
        assert symbol in graph_writer, f"Foundation模板合同未被真实Editor创作代码引用：{name}"
    assert "SetGraphLocal" in actor_code and "BindPlacedActorGraph" in creator, \
        "放置器需要把Graph（图资产）实际绑定给原生PCGComponent（PCG组件）"
    assert "bFillInterior" in masks_header and "Mask.bFillInterior && InsidePolygon" in masks_code, \
        "闭合围栏不得被误识别成清空整个农田内部的实心排除"

    config = text(GAME_CONFIG)
    assert '(Path="/Game/Development/Foundation/PCG/Definitions")' in config, \
        "GamePlatformData主资产扫描漏了金标准定义目录"
    assert '+DirectoriesToNeverCook=(Path="/Game/Development/Foundation")' in config, \
        "正式Client/Server Cook不应打包Foundation开发地图和占位资产"
    pipeline = text(TOOLS / "RunGoldLevelAuthoring.ps1")
    for stage in ("01-FoundationGraphs", "02-VillageBlueprints", "03-GoldDefinitions",
                  "04-RealizedSpawnerGraphs", "05-GoldLevelMap", "06-FreshEditorReopen"):
        assert stage in pipeline, "真实UE资产生成/回读缺少阶段：" + stage
    assert ('-EnablePlugins=PythonScriptPlugin,CommonUI,DBAGameplay' in pipeline and
            '-EnablePlugins=CommonUI,DBAGameplay,DBAWorlds,DBAWorldPack_Village' in pipeline), \
        "UE5.8 Python/Gold地图阶段必须使用实际依赖闭包的复数EnablePlugins参数，不得退回旧单插件约束"
    assert "'-Apply'" not in pipeline or "if (!$Apply)" in pipeline, \
        "默认模式必须只读，真实创建须明确Apply"

    check_crop_spline_source_contract()
    check_blueprint_root_contracts(pipeline)
    print("PCG_GOLD_AUTHORING_STATIC_CONTRACTS_PASS")
    print("TEMPLATES=12 SUBGRAPHS=7 BLUEPRINTS=11 DEFINITIONS=17 REALIZED=3 PROFILES=3")


if __name__ == "__main__":
    check()
