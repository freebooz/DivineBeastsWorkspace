# -*- coding: utf-8 -*-
"""未羊V6技能纹理：仅限真实Monolith MCP连接的UE5.8编辑器调用。
使用前必须校验24份PNG与唯一UIProfile。本工具不授予GAS能力。
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import unreal

ROOT=Path(__file__).resolve().parents[3]
PACK=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Goat"
OUT=ROOT/"Saved/Validation/GoatV6IconImport20261020"
PROFILE="/DBAHeroPack_Goat/UI/Abilities/DA_DBA_Goat_PlaceholderUIProfile"
MODE=str(globals().get("GOAT_V6_MODE","inspect"))

def check(ok,msg):
    """遇到任何身份、路径、图片或原生资源异常时阻断。"""
    if not ok:
        raise RuntimeError("SSLM_GOAT_V6: "+str(msg))

def read_inputs():
    """只处理八个冻结技能，V5正式名称不得由V6待审美术替代。"""
    freeze=json.loads((PACK/"Docs/GoatV6_ArtFreeze_20261020.json").read_text(encoding="utf-8"))
    plan=json.loads((OUT/"IntegrationPlan.json").read_text(encoding="utf-8"))
    check(freeze["heroId"]=="Hero.Zodiac.Goat" and len(freeze["skills"])==8,"身份不正确")
    check(len(plan["heroes"])==1 and plan["heroes"][0]["heroId"]=="Hero.Zodiac.Goat","接线计划错误")
    for skill,row in zip(freeze["skills"],plan["heroes"][0]["entries"]):
        check(skill["abilityId"]==row["abilityId"] and skill["texturePath"]==row["texturePath"],"映射错误")
        check(skill["productionV5NameZh"]==row["displayNameZh"],"不得覆盖正式名称")
        for key,size,hash_field in (("sourceFile",1254,"source"),("png64",64,"png64"),("png48",48,"png48")):
            file=(ROOT/skill[key]).resolve()
            check(file.is_relative_to(ROOT) and file.is_file(),"真实图源缺失")
            data=file.read_bytes()
            check(data[:8]==bytes.fromhex("89504e470d0a1a0a"),"PNG格式错误")
            check(struct.unpack(">II",data[16:24])==(size,size),"PNG尺寸错误")
            check(hashlib.sha256(data).hexdigest()==skill["sha256"][hash_field],"图源SHA错误")
    check(Path(unreal.Paths.get_project_file_path()).resolve()==(ROOT/"Game/DivineBeastsArena.uproject").resolve(),"工程错误")
    check(unreal.SystemLibrary.get_engine_version().startswith("5.8."),"引擎版本错误")
    return freeze,plan

def verify_native_textures(skills):
    """原生纹理审核：不以磁盘PNG存在代替UE5.8成功导入。"""
    lib=unreal.EditorAssetLibrary
    passed=[]
    for skill in skills:
        pkg=skill["texturePath"]
        tex=lib.load_asset(pkg)
        check(isinstance(tex,unreal.Texture2D),"真实Texture2D缺失："+pkg)
        shape=str(lib.find_asset_data(pkg).get_tag_value("Dimensions")).lower()
        check(shape=="48x48","真实运行纹理不为48像素："+pkg+" "+shape)
        check(tex.get_editor_property("srgb"),"sRGB未启用："+pkg)
        check(tex.get_editor_property("compression_settings")==unreal.TextureCompressionSettings.TC_EDITOR_ICON,
              "UI图标压缩设置错误："+pkg)
        check(tex.get_editor_property("mip_gen_settings")==unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
              "UI图标不得生成Mip："+pkg)
        passed.append({"path":pkg,"dimensions":shape})
    return passed


def import_preview(skills):
    """Monolith执行原生导入；任何已有同名路径都停止，不覆盖旧资产。"""
    lib=unreal.EditorAssetLibrary
    check(all(not lib.does_asset_exist(x["texturePath"]) for x in skills),
          "至少一个V6目标已存在；请人工审查或切换verify，不允许无条件覆盖")
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    for skill in skills:
        pkg=skill["texturePath"]
        folder,name=pkg.rsplit("/",1)
        task=unreal.AssetImportTask()
        task.set_editor_property("filename",str(ROOT/skill["png48"]))
        task.set_editor_property("destination_path",folder)
        task.set_editor_property("destination_name",name)
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("automated",True)
        task.set_editor_property("save",False)
        tools.import_asset_tasks([task])
        tex=lib.load_asset(pkg)
        check(isinstance(tex,unreal.Texture2D),"Texture2D导入失败："+pkg)
        tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group",unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings",unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter",unreal.TextureFilter.TF_BILINEAR)
        tex.set_editor_property("never_stream",True)
        tex.set_editor_property("srgb",True)
        check(lib.save_loaded_asset(tex,only_if_is_dirty=False),"Texture2D保存失败："+pkg)
    return verify_native_textures(skills)
