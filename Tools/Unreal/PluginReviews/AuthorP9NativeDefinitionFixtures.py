#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 原生P9 Data/Flow/Loading集成测试的两份真实开发Definition夹具作者。

真实资产仅创建在/Game/Development/Foundation/Definitions/P9Native（原工程禁止正式Cook）。
- GamePlatformDefinition:foundation.probe@1：通用Data/Loading租约，不包含项目玩法；
- GamePlatformDefinition:foundation.flow@1：真实单节点Flow Definition，执行器由集成测试运行时注入。
不写入生产AssetManager配置、不覆盖已经存在的别人的资产，不启用模拟Data服务。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
FOLDER="/Game/Development/Foundation/Definitions/P9Native"
OUT=ROOT/"Saved/Validation/PluginP7P9_20261012/P9DataFixtures"
MODE=os.environ.get("P9_NATIVE_FIXTURE_MODE","inspect").lower()
SPECS=(
    ("DA_GP_P9_FoundationProbe","probe","GamePlatformDefinitionBase"),
    ("DA_GP_P9_FoundationFlow","flow","GamePlatformFlowDefinition"),
)


def check_environment():
    proj=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not proj.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝在非DivineBeastsWorkspace工程创建夹具")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("不是锁定UE5.8")
    if MODE not in ("inspect","apply"):
        raise RuntimeError("P9_NATIVE_FIXTURE_MODE仅允许inspect/apply")
    ini=(ROOT/"Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if 'DirectoriesToNeverCook=(Path="/Game/Development/Foundation")' not in ini:
        raise RuntimeError("缺少禁止开发夹具进入发布包的保护")
    if '(Path="/Game/Development/Foundation/Definitions")' not in ini:
        raise RuntimeError("未登记正式数据类型的开发Definition扫描根；不可假定租约有效")


def new_logical_id(name):
    value=unreal.GamePlatformId()
    value.set_editor_property("namespace","foundation")
    value.set_editor_property("name",name)
    value.set_editor_property("logical_version",1)
    return value


def get_struct_name(value):
    return str(value).lower()


def verify(asset,name,path):
    if not isinstance(asset,unreal.GamePlatformDefinitionBase):
        raise TypeError("真实Definition资产类型不符："+path)
    identity=asset.get_editor_property("logical_id")
    if (identity.get_editor_property("namespace")!="foundation" or
        identity.get_editor_property("name")!=name or
        identity.get_editor_property("logical_version")!=1):
        raise ValueError("夹具身份被修改，拒绝覆盖："+path)
    version=asset.get_editor_property("data_version")
    if (version.get_editor_property("schema_version")!=1 or
        version.get_editor_property("content_revision")!=1):
        raise RuntimeError("夹具结构/内容版本不合法："+path)
    if name=="flow":
        if not isinstance(asset,unreal.GamePlatformFlowDefinition):
            raise TypeError("流程定义必须是真实的UGamePlatformFlowDefinition")
        if str(asset.get_editor_property("entry_node_id"))!="ReviewStart":
            raise RuntimeError("流程入口不符合独立测试夹具契约")
        nodes=asset.get_editor_property("nodes")
        if len(nodes)!=1 or str(nodes[0].get_editor_property("node_id"))!="ReviewStart" or str(nodes[0].get_editor_property("executor_id"))!="ReviewCallback":
            raise RuntimeError("流程节点或执行器身份不一致")
    return {"name":name,"path":path,"class":asset.get_class().get_name(),
            "logicalId":"GamePlatformDefinition:foundation."+name+"@1",
            "version":{"schema":1,"revision":1}}


def make_asset(name,kind,classname):
    path=FOLDER+"/"+name
    klass=getattr(unreal,classname,None)
    if klass is None:
        raise RuntimeError("缺少已编译UE原生类 "+classname)
    lib=unreal.EditorAssetLibrary
    created=False
    if lib.does_asset_exist(path):
        asset=lib.load_asset(path)
        if not isinstance(asset,klass):
            raise RuntimeError("已占用路径存在不同类型UE资产："+path)
    elif MODE=="apply":
        factory=unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class",klass)
        asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,FOLDER,klass,factory)
        if not isinstance(asset,klass):
            raise RuntimeError("真实UE DataAsset创建失败："+path)
        asset.set_editor_property("logical_id",new_logical_id(kind))
        asset.set_editor_property("data_version",unreal.GamePlatformDataVersion())
        asset.set_editor_property("required_definitions",[])
        if kind=="flow":
            # UE5.8 中此结构字段为 EditDefaultsOnly，实例 set_editor_property 不允许写；
            # 使用反射构造器提供真实节点字段，不绕过Definition生产校验。
            node=unreal.GamePlatformFlowNodeDefinition(
                node_id=unreal.Name("ReviewStart"),
                executor_id=unreal.Name("ReviewCallback"),
                timeout_seconds=15.0)
            asset.set_editor_property("entry_node_id",unreal.Name("ReviewStart"))
            asset.set_editor_property("nodes",[node])
            asset.set_editor_property("allow_cycles",False)
        if not lib.save_loaded_asset(asset,only_if_is_dirty=False):
            raise RuntimeError("未能保存真实Definition："+path)
        created=True
        print("P9_NATIVE_DEFINITION_CREATED",path)
    else:
        raise RuntimeError("真实Definition尚未制作："+path)
    info=verify(asset,kind,path)
    info["created"]=created
    return info


def main():
    check_environment()
    if MODE=="apply" and not unreal.EditorAssetLibrary.does_directory_exist(FOLDER):
        if not unreal.EditorAssetLibrary.make_directory(FOLDER):
            raise RuntimeError("无法创建开发测试定义目录")
    records=[make_asset(name,kind,cls) for name,kind,cls in SPECS]
    OUT.mkdir(parents=True,exist_ok=True)
    result={"engine":str(unreal.SystemLibrary.get_engine_version()),"mode":MODE,
            "folder":FOLDER,"defs":records,"count":len(records),
            "releaseCook":"excluded","manualAcceptance":"not_applicable",
            "scope":"真实开发集成夹具，不是生产逻辑/玩家数据"}
    (OUT/("Author_"+MODE+".json")).write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("P9_NATIVE_DEFINITION_BOTH_OK",len(records),MODE)


if __name__=="__main__":
    main()
