#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ANIMAL VARIETY PACK 仅限已授权原生资源的三层内容包迁移作者。

仅在Fab正式导入后处理已有的UE资产，绝不下载绕过Epic授权、不伪造uasset或编辑源素材版权。
- inspect（默认）：打印缺失输入与预期路径，不改动项目。
- prepare：UE编辑器内核实真实Wolf骨骼/序列与用户自行保存的许可凭证，准备可被下次编辑器挂载的纯内容插件描述文件；未取得资源不产生空插件。
- migrate：在重启并显式启用该内容插件后，由UE编辑器原生API移动完整的Fab源目录，迁移成功才登记uproject与内容包索引。
注意：本工具不运行编译、Cook、蓝图验证、PIE；后续仍需真实AnimBP制作。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

WORKSPACE=Path(__file__).resolve().parents[3]
MANIFEST=Path(__file__).with_name("LicensedAnimalVarietyImport.json")
SPEC=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
MODE=os.environ.get("SSLM_ANIMAL_PACK_MODE","inspect").strip().lower()


def require(predicate, message):
    if not predicate:
        raise RuntimeError(message)


def paths():
    dest=SPEC["Destination"]
    plugin_rel=Path(dest["PluginRelativeRoot"])
    require(not plugin_rel.is_absolute() and ".." not in plugin_rel.parts,
            "禁止出项目目录操作插件")
    plugin=(WORKSPACE/plugin_rel).resolve()
    require(plugin.is_relative_to(WORKSPACE.resolve()),"资产插件目标必须位于正式工作空间")
    require(dest["PluginName"]=="DBAContentPack_AnimalsProtofactor",
            "不得改写其它插件身份")
    return plugin, dest["PluginName"]


def inspect():
    inputs=SPEC["Source"]
    expected=["FabImportedContentDirectory","WolfSkeletalMesh","WolfSkeleton","WolfPhysicsAsset","WolfAnimationBlueprint"]
    missing=[key for key in expected if not inputs.get(key)]
    missing.extend("Animation."+key for key,val in inputs["WolfAnimationSequences"].items() if not val)
    permit=SPEC["Permission"]
    if not permit["LicenseRecordLocalPath"]:missing.append("Permission.LicenseRecordLocalPath")
    print("ANIMAL_VARIETY_PACK_STATUS",json.dumps({
        "模式":"只读inspect","源资源":SPEC["Status"],"缺失前置":missing,
        "内容插件":SPEC["Destination"]["PluginName"],"创建插件":False,
        "生成动画蓝图":False,"运行编译或Cook":False
    },ensure_ascii=False))


def resolve_editor():
    import unreal
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."),
            "本工具只允许UE5.8编辑器")
    project=Path(unreal.Paths.get_project_file_path()).resolve()
    require(project==(WORKSPACE/"Game/DivineBeastsArena.uproject").resolve(),
            "必须在SSLM正式游戏工程操作")
    require("-SSLMAnimalVarietyAuthoring" in unreal.SystemLibrary.get_command_line(),
            "需要明确的本次资源作者操作许可参数")
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor is not None and editor.get_game_world() is None,
            "PIE或游戏世界运行中不能迁移Fab资产")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() and
            not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),
            "现有工作区UE资产未保存，防止覆盖其他作者")
    return unreal


def preflight(unreal):
    """必须先拥有真实授权文件和Fab资产，不接收文本假路径。"""
    permit=SPEC["Permission"]
    receipt=permit["LicenseRecordLocalPath"]
    require(bool(receipt),"Fab资源尚未领取；缺少许可证明路径")
    receipt_path=Path(receipt).resolve()
    require(receipt_path.is_file() and receipt_path.stat().st_size>0,
            "真实Fab许可凭证不存在/为空，禁止新建插件")
    require(bool(permit["LicenseType"]) and bool(permit["AcquisitionDate"]),
            "必须填写真实授权类型及领取日期")
    src=SPEC["Source"]
    directory=src["FabImportedContentDirectory"].rstrip("/")
    require(directory.startswith("/Game/") and
            "/../" not in directory and len(directory)>len("/Game/"),
            "源素材须先由正版Fab插件导入Game下独立目录")
    lib=unreal.EditorAssetLibrary
    require(lib.does_directory_exist(directory),"Fab原生内容目录不存在")
    list_source=lib.list_assets(directory,recursive=True,include_folder=False)
    require(len(list_source)>1,"Fab导入目录内未找到真实资产集合")
    source_paths=[src["WolfSkeletalMesh"],src["WolfSkeleton"],src["WolfPhysicsAsset"],src["WolfAnimationBlueprint"]]
    source_paths.extend(v for v in src["WolfAnimationSequences"].values() if v)
    require(all(isinstance(x,str) and x.startswith(directory+"/")
                for x in source_paths),"狼模型/骨架/动画应全在本次待迁移的源目录下")
    mesh=lib.load_asset(src["WolfSkeletalMesh"])
    skeleton=lib.load_asset(src["WolfSkeleton"])
    require(isinstance(mesh,unreal.SkeletalMesh) and isinstance(skeleton,unreal.Skeleton),
            "源目录没有真正的狼骨骼网格与骨架")
    require(mesh.get_editor_property("skeleton")==skeleton,
            "狼网格和Skeleton不兼容")
    physics=lib.load_asset(src["WolfPhysicsAsset"])
    blueprint=lib.load_asset(src["WolfAnimationBlueprint"])
    require(isinstance(physics,unreal.PhysicsAsset),
            "狼的真实PhysicsAsset未正确导入")
    require(isinstance(blueprint,unreal.AnimBlueprint),
            "缺少原资源中的正式狼AnimBlueprint；应先制作与当前Skeleton一致的动画状态机")
    require(blueprint.get_editor_property("target_skeleton")==skeleton,
            "狼Animation Blueprint与骨骼Skeleton不匹配")
    # 一次只迁移原包全目录，避免逐个移动造成材质和动画悬空引用。
    for label,path in src["WolfAnimationSequences"].items():
        require(path,"缺少四足狼关键动作："+label)
        item=lib.load_asset(path)
        require(isinstance(item,unreal.AnimSequence),
                "缺少真实动画序列："+label+" "+path)
        require(item.get_editor_property("skeleton")==skeleton,
                "动画骨架不匹配："+label)
    return directory, list_source


def descriptor():
    _,name=paths()
    return {
        "FileVersion":3,"Version":1,"VersionName":"1.0.0",
        "FriendlyName":name,
        "Description":"SSLM通用动物授权美术包：仅客户端/编辑器加载狼、鹿、猪等资产，不承载怪物AI或服务器玩法。",
        "Category":"DivineBeasts.Content.Common",
        "CreatedBy":"DivineBeastsWorkspace",
        "CanContainContent":True,
        "EnabledByDefault":False,
        "Modules":[]
    }


def prepare(unreal):
    source, assets=preflight(unreal)
    plugin,name=paths()
    path=plugin/(name+".uplugin")
    require(not path.exists(),"已有同名插件；需要使用迁移阶段，不能覆盖原插件")
    require(not plugin.exists(),"目标通用动物包目录已经存在；拒绝覆盖")
    # 经过真实源资产及许可证预检后才可以留下插件描述文件；需重启Editor以挂载。
    plugin.mkdir(parents=True,exist_ok=False)
    (plugin/"Content").mkdir()
    path.write_text(json.dumps(descriptor(),ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_ANIMAL_PACK_PREPARED_RESTART_EDITOR",json.dumps({
        "plugin":str(path),"source":source,"sourceAssetCount":len(assets),
        "next":"重启UE编辑器并显式 -EnablePlugins=DBAContentPack_AnimalsProtofactor，再执行 migrate；当前尚未登记。"
    },ensure_ascii=False))


def atomic_json_write(path,data):
    temp=path.with_suffix(path.suffix+".wolf.tmp")
    require(not temp.exists(),"存在同名修改临时文件，禁止竞争写入："+str(temp))
    try:
        temp.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        temp.replace(path)
    finally:
        if temp.exists():temp.unlink()


def migrate(unreal):
    source, assets=preflight(unreal)
    plugin,name=paths()
    descriptor_file=plugin/(name+".uplugin")
    require(descriptor_file.is_file(),"通用动物插件尚未准备，先调用prepare并重启Editor")
    dest=SPEC["Destination"]["ImportedAssetFolder"]
    require(dest=="/"+name+"/Animals","目标挂载路径不符")
    lib=unreal.EditorAssetLibrary
    require(not lib.does_directory_exist(dest),"目标动物目录已存在，不允许覆盖")
    registry=WORKSPACE/"Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json"
    project=WORKSPACE/"Game/DivineBeastsArena.uproject"
    cfg=WORKSPACE/"Game/Config/Custom/VillageDevelopmentClient/DefaultGame.ini"
    obj=json.loads(project.read_text(encoding="utf-8-sig"))
    existing=[p for p in obj.get("Plugins",[]) if p.get("Name")==name]
    require(not existing,"插件已在主项目启用，禁止重复登记")
    registry_data=json.loads(registry.read_text(encoding="utf-8-sig"))
    require(not any(x["Name"]==name for x in registry_data["ContentPacks"]),
            "已注册同名内容包")
    # 通过Editor移动完整原包，让UE改写资源软硬引用；不得直接移动.uasset文件。
    require(lib.rename_directory(source,dest),
            "Editor未能移动真实Fab动物资源目录；仍不修改uproject和注册表")
    require(lib.does_asset_exist(SPEC["Source"]["WolfSkeletalMesh"].replace(source,dest,1)),
            "Fab狼骨骼迁移后并未产生真实资源；必须人工修复中断状态，不继续注册")

    rel="Common/"+name
    registry_data["ContentPacks"].append({"Name":name,"RelativePath":rel})
    obj["Plugins"].append({
        "Name":name,"Enabled":True,"TargetAllowList":["Game","Client","Editor"]
    })
    # 注册只在真实资产已迁移后执行。配置软资源路径仍需经过人工动画蓝图检查才可批准。
    atomic_json_write(registry,registry_data)
    atomic_json_write(project,obj)
    # Village开发客户端应该明确烘焙动物网格/贴图/动画；服务器插件装配不包含该内容包。
    rule='+DirectoriesToAlwaysCook=(Path="'+"/"+name+'/Animals")'
    value=cfg.read_text(encoding="utf-8-sig")
    if rule not in value:
        cfg.write_text(value+"\n; 已授权通用动物包仅供新手村开发客户端演示，专服不携带美术。\n"+rule+"\n",encoding="utf-8")
    print("SSLM_ANIMAL_PACK_MOVED_AND_REGISTERED",json.dumps({
        "source":source,"destination":dest,"migratedAssetCount":len(assets),
        "mesh":SPEC["Source"]["WolfSkeletalMesh"].replace(source,dest,1),
        "animationBlueprintStatus":"仍需按真实Skeleton制作与审核"
    },ensure_ascii=False))


def main():
    require(SPEC["SchemaVersion"]==1 and SPEC["Project"]=="SSLM","作者合同身份不符")
    require(MODE in ("inspect","prepare","migrate"),"不接受其它迁移模式")
    if MODE=="inspect":
        inspect()
        return
    unreal=resolve_editor()
    if MODE=="prepare":prepare(unreal)
    else:migrate(unreal)


if __name__=="__main__":
    main()
