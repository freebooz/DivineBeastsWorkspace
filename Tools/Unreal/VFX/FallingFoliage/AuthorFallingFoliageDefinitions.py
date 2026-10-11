#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""使用UE5.8制作四种真实UGamePlatformVFXWorldDefinition主数据资产。
依赖现有GamePlatformData身份合同和GamePlatformVFX Niagara系统。
拒绝缺资源和覆盖已有项目Definition；不虚构资产/逻辑ID。
"""
from __future__ import annotations
import os

MODE = os.environ.get("FOLIAGE_DEFINITIONS_MODE","inspect").lower()
ROOT = "/DBAPresentationPack_Core/VFX/Environment/FallingFoliage"
DEST = ROOT+"/Definitions"
EFFECT_TYPE = "/GamePlatformVFX/Environment/FallingFoliage/EffectTypes/NET_GP_FallingFoliage_Ambient"
KINDS = ("Peach", "Maple", "Bamboo", "Ginkgo")


def logical(value, unreal):
    namespace_and_name,version=value.rsplit("@",1)
    namespace,name=namespace_and_name.rsplit(".",1)
    ident=unreal.GamePlatformId()
    ident.set_editor_property("namespace",namespace)
    ident.set_editor_property("name",name)
    ident.set_editor_property("logical_version",int(version))
    return ident


def main():
    targets=[
        (kind,
         ROOT+f"/Niagara/NS_DBA_FallingFoliage_{kind}",
         f"presentation.dba.environment.fallingfoliage.{kind.lower()}@1",
         DEST+f"/DA_DBA_VFX_FallingFoliage_{kind}")
        for kind in KINDS
    ]
    if MODE!="apply":
        for kind,ns,ident,asset in targets:
            print("FOLIAGE_DEFINITION_PLANNED",kind,ns,ident,asset)
        return
    import unreal # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("仅允许神兽联盟UE5.8正式工程")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("必须使用UE5.8")
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    effect=assets.load_asset(EFFECT_TYPE)
    if not isinstance(effect,unreal.NiagaraEffectType):
        raise FileNotFoundError("缺少正式Niagara EffectType："+EFFECT_TYPE)
    loaded={}
    for kind,path,_,dst in targets:
        if assets.does_asset_exist(dst):
            raise FileExistsError("禁止覆盖已有Definition："+dst)
        system=assets.load_asset(path)
        if not isinstance(system,unreal.NiagaraSystem):
            raise FileNotFoundError("Niagara系统不存在或类型异常："+path)
        loaded[kind]=system
    if not assets.does_directory_exist(DEST):
        assets.make_directory(DEST)

    klass=unreal.GamePlatformVFXWorldDefinition
    factory=unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class",klass)
    for kind,system_path,ident,path in targets:
        d=tools.create_asset(path.rsplit("/",1)[1],DEST,klass,factory)
        if not isinstance(d,klass):
            raise RuntimeError("未生成真实UGamePlatformVFXWorldDefinition："+kind)
        d.set_editor_property("logical_id",logical(ident,unreal))
        d.set_editor_property("data_version",unreal.GamePlatformDataVersion())
        d.set_editor_property("niagara_system",loaded[kind])
        d.set_editor_property("effect_type",effect)
        d.set_editor_property("content_category",unreal.GamePlatformVFXContentCategory.ENVIRONMENT)
        d.set_editor_property("auto_destroy",False)
        d.set_editor_property("allow_pooling",True)
        d.set_editor_property("enable_scalability",True)
        d.set_editor_property("require_fixed_bounds",True)
        d.set_editor_property("max_lifetime_seconds",0.0)
        # 请求覆盖白名单仅包含动态接触面；其它风、生成率、旋转等由变体默认值持有。
        # USTRUCT成员是EditDefaultsOnly：UE Python拒绝set_editor_property修改独立结构体。
        # 通过反射导入文本建立值快照，再一次性赋给真正的DataAsset。
        schema=unreal.GamePlatformVFXParameterSchema()
        schema_text='(MaxOverrideCount=8,Rules=((Name="User.FoliageGroundHeight",Type=Float,bRequired=True,MinValue=-10000000.0,MaxValue=10000000.0)))'
        if not schema.import_text(schema_text):
            raise RuntimeError("参数白名单导入失败："+schema_text)
        d.set_editor_property("parameter_schema",schema)
        if not assets.save_loaded_asset(d,only_if_is_dirty=False):
            raise RuntimeError("Definition写盘失败："+path)
        obj=assets.load_asset(path)
        if not isinstance(obj,klass):
            raise RuntimeError("Definition回读失败："+path)
        print("FOLIAGE_DEFINITION_REAL_UASSET_SAVED",kind,obj.get_path_name())
    print("FOLIAGE_DEFINITIONS_APPLY_COMPLETED",len(targets))


if __name__=="__main__":
    main()
