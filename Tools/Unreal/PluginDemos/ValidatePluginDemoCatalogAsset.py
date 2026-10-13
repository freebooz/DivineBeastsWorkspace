#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟插件门户的正式编目DataAsset（数据资产）独立UE5.8回读。

只读检查：编辑器版本、工程身份、资产真实类型、作者SHA、65插件身份以及
功能/案例/审查合同数量。单凭文件存在或旧Editor DLL不算验证通过。
不改动任何UObject资源；证据仅写入Saved/Validation（临时验证目录）。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal  # type: ignore

WORKSPACE=Path(__file__).resolve().parents[3]
PROJECT=WORKSPACE/"Game/DivineBeastsArena.uproject"
AUTHOR=WORKSPACE/"Docs/Implementation/PluginDemos/PluginDemoManifest.json"
PACKAGE="/DBAUIPack_Core/Development/PluginDemos/Definitions/DA_DBA_PluginDemoCatalog"
OUTPUT=WORKSPACE/"Saved/Validation/DBAPluginDevDualMode/PluginDemoCatalogAssetReadback.json"


def main():
    """只读取已保存的真实资产；版本、数目与SHA不一致即失败，不能写入部分通过记录。"""
    actual=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if actual!=PROJECT.resolve():
        raise RuntimeError("当前编辑器不是正式DivineBeastsWorkspace工程，拒绝验收")
    engine=str(unreal.SystemLibrary.get_engine_version())
    if not engine.startswith("5.8."):
        raise RuntimeError("本测试只允许虚幻引擎5.8")
    reference=AUTHOR.read_bytes()
    manifest=json.loads(reference.decode("utf-8-sig"))
    fingerprint=hashlib.sha256(reference).hexdigest()
    klass=getattr(unreal,"DivineBeastsDemoCatalogDefinition",None)
    if klass is None:
        raise RuntimeError("Editor尚未装载正式编目UClass，不能用旧DLL回读")
    asset=unreal.EditorAssetLibrary.load_asset(PACKAGE)
    if not isinstance(asset,klass):
        raise RuntimeError("实际UE资产不存在或继承的C++类型错误："+PACKAGE)
    if asset.get_editor_property("source_fingerprint")!=fingerprint:
        raise RuntimeError("资产与本次作者JSON SHA256不匹配，禁止沿用旧定义")
    expected={
        "plugins":len(manifest["plugins"]),
        "features":len(manifest["features"]),
        "scenarios":len(manifest["scenarios"]),
        "review_cases":len(manifest["review_cases"]),
        "legacy_map_routes":len(manifest["legacy_map_routes"]),
    }
    for name,total in expected.items():
        actual_count=len(asset.get_editor_property(name))
        if actual_count!=total:
            raise RuntimeError("资产"+name+"完整数量不一致："+str(actual_count)+"/"+str(total))
    plugins=asset.get_editor_property("plugins")
    actual_ids={str(item.get_editor_property("plugin_id")) for item in plugins}
    expected_ids={item["plugin_id"] for item in manifest["plugins"]}
    if actual_ids!=expected_ids:
        raise RuntimeError("65个真实插件的身份不一致，不能宣称已完成编目")
    version=asset.get_editor_property("data_version")
    if version.get_editor_property("schema_version")!=2:
        raise RuntimeError("真实资产Schema（结构版本）不是v2")
    ids=asset.get_editor_property("logical_id")
    if str(ids.get_editor_property("namespace"))!="divinebeasts.demo" or str(ids.get_editor_property("name"))!="catalog":
        raise RuntimeError("主资产身份不属于项目正式开发目录")
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    result={"status":"passed","engine":engine,"project":str(PROJECT),"asset":PACKAGE,
            "sourceFingerprint":fingerprint,"counts":expected,"independentReload":True,
            "runtimeCatalogReady":"not_tested","widgetMonolithAudit":"pending",
            "humanAcceptance":"pending"}
    OUTPUT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("DBA_DEMO_CATALOG_ASSET_RELOAD_PASSED",json.dumps(expected,ensure_ascii=False))


if __name__=="__main__":
    main()
