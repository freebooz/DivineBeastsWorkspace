#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 通用材质函数库201项可追溯清单索引与真实资源盘点。

职责：从既有201项候选台账+真实UE AssetRegistry读取各领域实际资源，
写入逐项原生类型/包路径/节点数/开发审核材质引用及证据限制，不修改uasset。
201项中仅一部分应创建UMaterialFunction，NativeReuse（引擎复用）和Other
（Niagara/UMG/PCG/母版等）不因没有MF路径被标记“缺失”。
只读inspect默认不写输出；scan仅在实际正确UE5.8主工程运行并写审计JSON+中文表。
Shader、Cook、GPU性能和人工审查并非AssetRegistry能判定；字段一律待独立证据，
不能生成虚假的生产验收或测试通过标识。
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
DOCS=ROOT/"Docs/Implementation/Materials"
REGISTRY=DOCS/"MaterialCapabilityRegistry_V1.json"
PROFILES=ROOT/"Tools/Unreal/Surface/MaterialLibraryLayerProfiles_V1.json"
OUTPUT=DOCS/"MaterialCapabilityAssetAudit_UE58.json"
INDEX=DOCS/"MaterialCapabilityStatus_UE58.md"


def project_asset_path(row,ml_paths):
    """使用冻结正式路径或具备明确归属的新路径，不猜想现成函数资源。"""
    frozen=row.get("ContractAssetPath")
    if frozen:return frozen
    if row["ImplementationMode"]=="ML":
        return ml_paths.get(row["CapabilityId"])
    if row["ImplementationMode"]=="MF":
        if row.get("ProposedAssetPath"):return row["ProposedAssetPath"]
        if row.get("OwnerPlugin")=="DBAPresentationPack_Core":
            return "/DBAPresentationPack_Core/MaterialFunctions/Character/MF_DBA_"+row["NameEN"]
    return None


def review_candidates(row):
    """只读审核材质有多个历史命名形态，必须以真实UE加载确认存在。"""
    name=row["NameEN"]
    aliases={"RadialEnergyGradient":"VFXRadialEnergy"}
    key=aliases.get(name,name)
    base="/Game/Development/MaterialLibrary/Materials/M_DBA_MaterialLibrary_"+key
    values=[base+"_Review",base+"_DomainReview"]
    if row["OwnerPlugin"]=="GamePlatformUI":
        values.append("/Game/Development/MaterialLibrary/UI/M_DBA_UI_"+name+"_Review")
    return values


def load_inputs():
    """读201台账并冻结7个正式既有MF合同，不按文件名机械推算全部实现。"""
    data=json.loads(REGISTRY.read_text(encoding="utf8"))
    profiles=json.loads(PROFILES.read_text(encoding="utf8"))
    rows=data.get("items",[])
    if len(rows)!=201 or len({r["CapabilityId"] for r in rows})!=201:
        raise RuntimeError("正式201项能力表编号未闭环，停止真实资产盘点")
    layers={x["capabilityId"]:x["materialLayerAssetPath"] for x in profiles["items"]}
    if len(layers)!=12:
        raise RuntimeError("MaterialLayer独立配方数量不是12")
    return rows,layers


def scan(unreal,rows,ml_paths):
    """只扫描已有资源挂载点；AssetRegistry存在不代表Shader/Cook和运行功能通过。"""
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("本工具拒绝盘点非正式UE工程："+project)
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/GamePlatformSurface","/GamePlatformVFX",
                                     "/GamePlatformUI","/DBAPresentationPack_Core",
                                     "/Game/Development/MaterialLibrary"],force_rescan=True)
    lib=unreal.EditorAssetLibrary
    edits=unreal.MaterialEditingLibrary
    observed=[]
    verified_count=0
    for row in rows:
        path=project_asset_path(row,ml_paths)
        mode=row["ImplementationMode"]
        kind=None
        count=None
        exposed=None
        description=None
        actual=None
        exists=False
        error=None
        if path:
            obj=lib.load_asset(path) if lib.does_asset_exist(path) else None
            exists=obj is not None
            if obj:
                actual=obj.get_class().get_name()
                kind="MaterialLayer" if "MaterialLayer" in actual else actual
                if isinstance(obj,unreal.MaterialFunction):
                    count=edits.get_num_material_expressions_in_function(obj)
                    try:exposed=bool(obj.get_editor_property("expose_to_library"))
                    except (AttributeError,RuntimeError,TypeError):exposed=None
                    try:description=str(obj.get_editor_property("description"))
                    except (AttributeError,RuntimeError,TypeError):description=None
                verified_count+=1
                expected_types=("MaterialFunctionMaterialLayer",) if mode=="ML" else ("MaterialFunction",)
                if mode in ("MF","ML","ExistingContract") and actual not in expected_types:
                    # UE5.8的MaterialFunctionMaterialLayer可能继承基类，仍用原生类型区分。
                    error="已有UE对象原生类别与索引承诺不匹配："+str(actual)
        reviews=[]
        for qa in review_candidates(row):
            if lib.does_asset_exist(qa):
                material=lib.load_asset(qa)
                if isinstance(material,unreal.Material):
                    reviews.append({
                        "package":qa,
                        "className":"Material",
                        "graphNodeCount":edits.get_num_material_expressions(material),
                        "shaderCompiledHere":"未由该只读盘点执行，等待统一编译证据"
                    })
        observed.append({
            "capabilityId":row["CapabilityId"],
            "nameZH":row["NameZH"],
            "nameEN":row["NameEN"],
            "category":row["Category"],
            "ownerLayer":row["OwnerLayer"],
            "ownerPlugin":row["OwnerPlugin"],
            "implementationMode":mode,
            "expectedAssetPath":path,
            "ueAssetExists":exists if path else None,
            "actualClass":actual,
            "expressionCount":count,
            "libraryExposed":exposed,
            "assetDescription":description,
            "reviewMaterials":reviews,
            "auditError":error,
            "sourceStatus":row.get("SourceStatus"),
            "shaderStatus":"请引用实际统一Shader编译报告，不由AssetRegistry猜测",
            "clientCook":"待真实Cook及暂存包审计",
            "serverCookStrip":"待专用服务器资源剥离实测",
            "humanVisualReview":"待人工亲自签收"
        })
    counts=Counter(x["implementationMode"] for x in observed)
    discovered=Counter(x["implementationMode"] for x in observed if x["ueAssetExists"] is True)
    return {
        "schemaVersion":1,
        "executedUTC":datetime.now(timezone.utc).isoformat(),
        "ueProject":project,
        "engineVersion":"UE5.8（虚幻引擎5.8）",
        "categoryCount":14,
        "capabilityCount":201,
        "candidateModeCounts":dict(counts),
        "nativeAssetDiscoveredModeCounts":dict(discovered),
        "totalNativeExpectedAssetFound":verified_count,
        "notesZH":"设计能力≠材质函数资源数量；这里只核验真实引擎加载的资产、类别及节点，不代替着色器、运行、GPU、Cook及人工审查。",
        "items":observed
    }


def render_index(result):
    """中文状态表由同一轮UE资产扫描结果生成，不复制旧静态表中的未验收结论。"""
    header=[
        "# 通用材质函数库201项真实资产状态索引（UE5.8）",
        "",
        "该索引由UE5.8实时AssetRegistry与原生资源加载记录自动生成。",
        "它只证明引擎资产存在/类型/节点；统一Shader编译、Cook、视觉签收仍需单独完成。",
        "",
        f"扫描记录时间（UTC）：{result['executedUTC']}。",
        f"登记设计能力：{result['capabilityCount']}项；加载确认实际原生资产：{result['totalNativeExpectedAssetFound']}项。",
        "",
        "| 能力ID | 中文功能 | 实现模式 | 所属插件 | 资产原生类型/结果 | 图节点 | 审核材质数 |",
        "|---|---|---|---|---|---:|---:|"
    ]
    for item in result["items"]:
        found=item["ueAssetExists"]
        status=(item["actualClass"] or "待制作") if found is not None else "原生功能或其他机制（不强制新MF）"
        header.append(
            "| "+item["capabilityId"]+" | "+item["nameZH"]+
            " | "+item["implementationMode"]+" | "+item["ownerPlugin"]+
            " | "+status+" | "+str(item["expressionCount"] or "—")+
            " | "+str(len(item["reviewMaterials"]))+" |")
    header.extend(["","完整分项机器证据：MaterialCapabilityAssetAudit_UE58.json。",
                   "不得将UE加载成功自动升为生产通过；人工Review待用户实际确认。",""])
    return "\n".join(header)


def main():
    """scan只改变Docs索引，不改项目正式二进制资产及现有Shader。"""
    cli=argparse.ArgumentParser(description="UE5.8平台通用材质201项真实状态索引扫描")
    cli.add_argument("--mode",choices=("inspect","scan"),default="inspect")
    options,_=cli.parse_known_args()
    rows,layers=load_inputs()
    print("MATERIAL_CAPABILITY_LEDGER",len(rows),"material_layer_profiles",len(layers))
    if options.mode=="inspect":
        source=Counter(r["ImplementationMode"] for r in rows)
        print("CANDIDATE_MODE_COUNTS",dict(source))
        print("INSPECT_ONLY：没有打开引擎、没有写入UE或台账文件")
        return
    try:import unreal # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("必须通过UE5.8正式编辑器做真实资源盘点") from error
    report=scan(unreal,rows,layers)
    DOCS.mkdir(parents=True,exist_ok=True)
    OUTPUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    INDEX.write_text(render_index(report),encoding="utf8")
    print("MATERIAL_CAPABILITY_AUDIT_SAVED",OUTPUT,"native_found",report["totalNativeExpectedAssetFound"])
    print("MATERIAL_CAPABILITY_INDEX_SAVED",INDEX,"rows",report["capabilityCount"])
    if any(i["auditError"] for i in report["items"]):
        raise RuntimeError("真实UE资产类型不匹配，已写入证据并阻断最终验收")


if __name__=="__main__":
    main()
