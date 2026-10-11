#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟CliffRock01青灰悬崖实用WorldAligned（世界对齐三向投射）PBR材质制作。

平台可复用原则：项目层只复用UE5.8引擎内置WorldAlignedTexture/WorldAlignedNormal，
不复制平台Surface公共函数、不修改现有十层材质或林地Rock01。
将蓝灰色BaseColor、原始NormalDX、原始ORM三个同源4K TextureObject，
经三轴世界投射成独立的默认光照UMaterial（切线空间法线输出关闭，按世界空间法线合成）。
世界单位cm，扫描原图宽度20m，对应CliffWorldTextureSizeCm=2000；
MI允许单独调整三轴尺度/岩色/粗糙度。注意世界三轴投射比简单UV多采样，
需在正式悬崖和目标GPU上测量预算后才能发布，现仅项目候选独立路径。
CLIFFROCK_WORLDALIGN_MODE=inspect/apply/verify；真实UE创建、编译、保存和只读回读。
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SRC=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
     "SourceArt/Terrain/PBR/CliffRock01")
MANIFEST=SRC/"CliffRock01_ColorGradeManifest.json"
FOLDER="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials"
MATERIAL=FOLDER+"/M_DBA_CliffRock01_WorldAligned"
INSTANCE=FOLDER+"/MI_DBA_CliffRock01_WorldAligned"
TEX="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Textures"
TEXTURES={
 "BaseColor":TEX+"/T_DBA_CliffRock01_BaseColor_BlueGray",
 "NormalDX":TEX+"/T_DBA_CliffRock01_NormalDX",
 "ORM":TEX+"/T_DBA_CliffRock01_ORM",
}
ENGINE_WORLD_TEXTURE="/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"
ENGINE_WORLD_NORMAL="/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal"
MODE=os.getenv("CLIFFROCK_WORLDALIGN_MODE","inspect").lower().strip()


def main()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("WorldAligned材质制作仅允许inspect/apply/verify")
    desc=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    target=(ROOT/desc["gradedSourcePath"]).resolve()
    if (desc.get("sourceId")!="marble_cliff_05" or
            desc.get("productionAssetApproved") is not False or
            not target.is_relative_to(SRC.resolve()) or
            not target.is_file() or
            hashlib.sha256(target.read_bytes()).hexdigest()!=desc["gradedBaseColorSHA256"]):
        raise RuntimeError("Cliff项目派生青灰4K扫描未经过实际签名")
    print("CLIFFROCK_WORLDALIGNED_PLAN",MODE,MATERIAL,INSTANCE)
    if MODE=="inspect":return
    try:
        import unreal # type: ignore
    except ImportError as e:
        raise RuntimeError("必须通过UE5.8真正Editor创建三向投射材质") from e
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前不在正式DivineBeastsArena项目")
    library=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    tool=unreal.AssetToolsHelpers.get_asset_tools()
    texture_objs={}
    for role,path in TEXTURES.items():
        tex=library.load_asset(path)
        if not isinstance(tex,unreal.Texture2D):
            raise TypeError("先制作并保存CliffRock01四张4K原生Texture2D："+path)
        texture_objs[role]=tex
    funcs={
        "BaseColor":unreal.load_asset(ENGINE_WORLD_TEXTURE),
        "NormalDX":unreal.load_asset(ENGINE_WORLD_NORMAL),
        "ORM":unreal.load_asset(ENGINE_WORLD_TEXTURE),
    }
    if not all(isinstance(fn,unreal.MaterialFunction) for fn in funcs.values()):
        raise FileNotFoundError("UE5.8的原生世界对齐材质函数缺失")
    if library.does_asset_exist(MATERIAL) and not isinstance(library.load_asset(MATERIAL),unreal.Material):
        raise TypeError("三向投射目标被其他类别资产占用")
    if library.does_asset_exist(INSTANCE) and not isinstance(
            library.load_asset(INSTANCE),unreal.MaterialInstanceConstant):
        raise TypeError("三向投射实例同名资产类别不符")
    if MODE=="verify" and (not library.does_asset_exist(MATERIAL) or
                           not library.does_asset_exist(INSTANCE)):
        raise FileNotFoundError("真实世界投射Material/MI还没有创建")
    # 仅恢复本工具独占路径下、因前次UE会话意外退出而留下的未完成临时材质图；
    # 对已保存但节点不全的本次CliffRock WorldAligned Material，先备份原始uasset，
    # 校验只含本工具预期节点后才重新连线；其它地图/材质/插件资源一律不触碰。
    source_material_file=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                           "DBAWorldPack_Village/Content/Terrain/Realism/CliffRock01/"
                           "Materials/M_DBA_CliffRock01_WorldAligned.uasset")
    old_material=library.load_asset(MATERIAL) if library.does_asset_exist(MATERIAL) else None
    old_count=(edit.get_num_material_expressions(old_material)
               if isinstance(old_material,unreal.Material) else 0)
    own_unsaved=(MODE=="apply" and isinstance(old_material,unreal.Material) and
                 not source_material_file.exists())
    own_partial_saved=(MODE=="apply" and isinstance(old_material,unreal.Material) and
                       source_material_file.is_file() and old_count<13)
    if MODE=="apply" and (not library.does_asset_exist(MATERIAL) or
                          own_unsaved or own_partial_saved):
        if not library.does_directory_exist(FOLDER):
            library.make_directory(FOLDER)
        if own_unsaved or own_partial_saved:
            mat=old_material
            if (not isinstance(mat,unreal.Material) or
                    old_count<7 or old_count>24 or
                    mat.get_path_name()!=MATERIAL+"."+MATERIAL.rsplit("/",1)[-1]):
                raise RuntimeError("本次WorldAligned未完成材质身份不符合安全恢复合同")
            if own_partial_saved:
                backup=(ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01/"
                        "Backups/M_DBA_CliffRock01_WorldAligned_BeforeRepair.uasset")
                if backup.exists():
                    if backup.read_bytes()!=source_material_file.read_bytes():
                        raise RuntimeError("原候选资产在备份后发生更改，拒绝覆盖")
                else:
                    backup.parent.mkdir(parents=True,exist_ok=True)
                    shutil.copy2(source_material_file,backup)
                print("CLIFFROCK_WORLDALIGNED_PARTIAL_ASSET_BACKUP",backup)
            edit.delete_all_material_expressions(mat)
            count_after=edit.get_num_material_expressions(mat)
            if count_after:
                # UE5.8中某些材质函数调用节点在DeleteAll后仍保留编辑器表达式；
                # 严格遍历本材质内部剩余节点，逐个调用官方DeleteMaterialExpression清理。
                print("CLIFFROCK_WORLDALIGNED_DELETE_ALL_REMAINDER",count_after)
                for expression in list(edit.get_material_expressions(mat)):
                    edit.delete_material_expression(mat,expression)
            if edit.get_num_material_expressions(mat)!=0:
                raise RuntimeError("清理本次CliffRock未完成材质图失败：残留节点="+
                                   str(edit.get_num_material_expressions(mat)))
            print("CLIFFROCK_WORLDALIGNED_OWN_PARTIAL_GRAPH_REBUILT",old_count)
        else:
            name=MATERIAL.rsplit("/",1)[-1]
            mat=tool.create_asset(name,FOLDER,unreal.Material,unreal.MaterialFactoryNew())
            if not isinstance(mat,unreal.Material):
                raise RuntimeError("UE未创建真实三向投射UMaterial")
        # 内置WorldAlignedNormal输出世界空间法线；禁止当切线空间向量错误使用。
        mat.set_editor_property("tangent_space_normal",False)
        def expression(cls,x,y):
            obj=edit.create_material_expression(mat,cls,int(x),int(y))
            if not obj:
                raise RuntimeError("世界投射真实材质节点未生成："+str(cls))
            return obj
        def link(left,right,slot,out=""):
            connected=edit.connect_material_expressions(left,out,right,slot)
            # UE5.8的ComponentMask输入引脚在反射层显示为None而不是Input；
            # 按官方材质连接API，以空输入名选择首个真实引脚。
            if not connected and slot=="Input":
                connected=edit.connect_material_expressions(left,out,right,"")
            if not connected:
                raise RuntimeError(f"WorldAligned引脚未连通 {left.get_name()}:{out} → {right.get_name()}:{slot}")
        tex_size=expression(unreal.MaterialExpressionVectorParameter,-1600,350)
        tex_size.set_editor_property("parameter_name",unreal.Name("CliffWorldTextureSizeCm"))
        tex_size.set_editor_property("default_value",unreal.LinearColor(2000,2000,2000,1))
        outputs={}
        for i,role in enumerate(("BaseColor","NormalDX","ORM")):
            y=(-800+i*700)
            texobj=expression(unreal.MaterialExpressionTextureObjectParameter,-1530,y)
            texobj.set_editor_property("parameter_name",unreal.Name("CliffWorld"+role))
            texobj.set_editor_property("texture",texture_objs[role])
            function=expression(unreal.MaterialExpressionMaterialFunctionCall,-1050,y+30)
            if not function.set_material_function(funcs[role]):
                raise RuntimeError("UE WorldAligned内置函数真实资产未绑定："+role)
            link(texobj,function,"TextureObject")
            link(tex_size,function,"TextureSize")
            outputs[role]=function
        tint=expression(unreal.MaterialExpressionVectorParameter,-560,-1070)
        tint.set_editor_property("parameter_name",unreal.Name("CliffWorldColorTint"))
        tint.set_editor_property("default_value",unreal.LinearColor(1,1,1,1))
        color=expression(unreal.MaterialExpressionMultiply,-230,-720)
        link(outputs["BaseColor"],color,"A","XYZ Texture")
        link(tint,color,"B")
        if not edit.connect_material_property(color,"",unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("Cliff WorldAligned BaseColor Shader输出未连接")
        if not edit.connect_material_property(outputs["NormalDX"],"XYZ Texture",
                                              unreal.MaterialProperty.MP_NORMAL):
            raise RuntimeError("Cliff WorldAligned世界法线Shader输出未连接")
        # 内置WorldAlignedTexture返回float3，分别拆出ARM物理通道。
        channels=(
            ("AO","r",unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
            ("Roughness","g",unreal.MaterialProperty.MP_ROUGHNESS),
            ("Metal","b",unreal.MaterialProperty.MP_METALLIC),
        )
        for idx,(channel,flag,prop) in enumerate(channels):
            mask=expression(unreal.MaterialExpressionComponentMask,-470,480+idx*190)
            mask.set_editor_property(flag,True)
            link(outputs["ORM"],mask,"Input","XYZ Texture")
            if channel=="Roughness":
                rough_scale=expression(unreal.MaterialExpressionScalarParameter,-280,765)
                rough_scale.set_editor_property("parameter_name",
                                                unreal.Name("CliffWorldRoughnessScale"))
                rough_scale.set_editor_property("default_value",1.0)
                scaled=expression(unreal.MaterialExpressionMultiply,-50,715)
                link(mask,scaled,"A")
                link(rough_scale,scaled,"B")
                shader_source=scaled
            else:
                shader_source=mask
            if not edit.connect_material_property(shader_source,"",prop):
                raise RuntimeError("世界对齐材质ARM拆包失败："+channel)
        cnt=edit.get_num_material_expressions(mat)
        if cnt<13:
            raise RuntimeError("WorldAligned三向函数和PBR节点不全："+str(cnt))
        diagnostics=edit.recompile_material(mat) or []
        for item in diagnostics:
            print("CLIFFROCK_WORLDALIGNED_SHADER_DIAGNOSTIC",str(item))
            if "error" in str(item).lower():
                raise RuntimeError("世界三向投射函数Shader编译错误")
        if not library.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("Cliff WorldAligned真实Material保存失败")
        print("CLIFFROCK_WORLDALIGNED_MATERIAL_SAVED",MATERIAL,"nodeCount",cnt)
    if MODE=="apply" and not library.does_asset_exist(INSTANCE):
        name=INSTANCE.rsplit("/",1)[-1]
        inst=tool.create_asset(name,FOLDER,unreal.MaterialInstanceConstant,
                               unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(inst,unreal.MaterialInstanceConstant):
            raise RuntimeError("UE三向投射材质实例创建失败")
        edit.set_material_instance_parent(inst,library.load_asset(MATERIAL))
        for role,tex in texture_objs.items():
            edit.set_material_instance_texture_parameter_value(inst,"CliffWorld"+role,tex)
        edit.set_material_instance_vector_parameter_value(
            inst,"CliffWorldTextureSizeCm",unreal.LinearColor(2000,2000,2000,1))
        edit.set_material_instance_scalar_parameter_value(inst,"CliffWorldRoughnessScale",1.0)
        if not library.save_loaded_asset(inst,only_if_is_dirty=False):
            raise RuntimeError("真实Cliff WorldAligned MI保存失败")
        print("CLIFFROCK_WORLDALIGNED_MI_SAVED",INSTANCE)
    mat=library.load_asset(MATERIAL)
    inst=library.load_asset(INSTANCE)
    if not isinstance(mat,unreal.Material) or not isinstance(inst,unreal.MaterialInstanceConstant):
        raise TypeError("WorldAligned原生UMaterial/MI不能加载")
    if mat.get_editor_property("tangent_space_normal"):
        raise RuntimeError("内置世界法线函数输出需要关闭TangentSpaceNormal")
    if inst.get_editor_property("parent")!=mat:
        raise RuntimeError("WorldAligned实例父材质不正确")
    for role,tex in texture_objs.items():
        if edit.get_material_instance_texture_parameter_value(inst,"CliffWorld"+role)!=tex:
            raise RuntimeError("三向投射材质纹理不匹配："+role)
    nodes=edit.get_num_material_expressions(mat)
    if nodes<13:
        raise RuntimeError("WorldAligned Shader节点丢失")
    print("CLIFFROCK_WORLDALIGNED_UE_VERIFY_PASS",
          "material=1 materialInstance=1 functions=3",
          "triplanarTextureProjection=True worldNormal=True",
          "textureScaleCm=2000 nodeCount",nodes,"manualApprovalPending=True")

if __name__=="__main__":
    main()
