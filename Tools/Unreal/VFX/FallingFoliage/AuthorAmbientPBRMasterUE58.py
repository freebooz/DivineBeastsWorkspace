# -*- coding: utf-8 -*-
"""UE5.8平台层PBR植被Sprite母材质作者脚本。

职责：为GamePlatformVFX（基础层视觉特效插件）创建可跨游戏的统一双面植被物理材质。
依赖：UE5.8原生MaterialEditingLibrary；不依赖任何神兽联盟纹理或项目定义。
副作用：首次运行时真实创建和保存平台Material .uasset；存在时只审查不覆盖。
失败：连接或编译失败记录JSON并抛异常，不生成空或假资产。
"""
from __future__ import annotations
import json, traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
PATH="/GamePlatformVFX/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage"
REPORT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/PlatformPBRMaster.json"
LIB=unreal.MaterialEditingLibrary
ED=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
OUTPUT={"asset":PATH,"created":False,"success":False,"errors":[],"engine":str(unreal.SystemLibrary.get_engine_version())}

def create(cls,parameter,x,y):
    node=LIB.create_material_expression(material,cls,x,y)
    if node is None:raise RuntimeError("编辑器材质图创建节点失败："+parameter)
    if parameter:
        node.set_editor_property("parameter_name",unreal.Name(parameter))
    return node

def edge(a,from_name,b,to_name):
    if not LIB.connect_material_expressions(a,from_name,b,to_name):
        raise RuntimeError("材质节点连接失败: "+to_name)

def port(node,output,property_value):
    if not LIB.connect_material_property(node,output,property_value):
        raise RuntimeError("材质输出连接失败: "+str(property_value))

try:
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject") or not OUTPUT["engine"].startswith("5.8."):
        raise RuntimeError("必须使用锁定的神兽联盟UE5.8")
    exists=ED.does_asset_exist(PATH)
    if exists:
        material=ED.load_asset(PATH)
        if not isinstance(material,unreal.Material):
            raise TypeError("同路径存在非Material资产，不得覆盖")
        OUTPUT["success"]=True
        OUTPUT["message"]="平台PBR母材质已存在，保留历史材质图"
        print("AMBIENT_PBR_MASTER_REUSED",PATH,flush=True)
    else:
        material=TOOLS.create_asset("M_GP_AmbientNature_PBR_Foliage",
            "/GamePlatformVFX/Environment/AmbientNature/Materials",
            unreal.Material,unreal.MaterialFactoryNew())
        if not isinstance(material,unreal.Material):
            raise RuntimeError("未创建真实UE5.8 Material")
        material.set_editor_property("two_sided",True)
        material.set_editor_property("used_with_niagara_sprites",True)
        material.set_editor_property("blend_mode",unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property("opacity_mask_clip_value",0.27)
        name=next((x for x in dir(unreal.MaterialShadingModel) if "TWO_SIDED_FOLIAGE" in x.upper()),None)
        if not name:raise RuntimeError("UE5.8缺少双面植被着色模型，不允许偷偷回退为Unlit")
        material.set_editor_property("shading_model",getattr(unreal.MaterialShadingModel,name))
        default_texture=ED.load_asset("/Engine/EngineResources/DefaultTexture")
        if not isinstance(default_texture,unreal.Texture):
            raise RuntimeError("引擎默认纹理缺失")
        albedo=create(unreal.MaterialExpressionTextureSampleParameter2D,"LeafAlbedo",-1160,-600)
        albedo.set_editor_property("texture",default_texture)
        normal=create(unreal.MaterialExpressionTextureSampleParameter2D,"LeafNormal",-1160,-270)
        # 第一次生成时使用中性Normal向量；正式MI会替换为合法TC_NORMALMAP。
        default_normal=ED.load_asset("/Engine/EngineMaterials/DefaultNormal")
        if isinstance(default_normal,unreal.Texture):
            normal.set_editor_property("texture",default_normal)
        else:
            normal.set_editor_property("texture",default_texture)
        normal.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        opacity=create(unreal.MaterialExpressionTextureSampleParameter2D,"LeafOpacity",-1160,65)
        opacity.set_editor_property("texture",default_texture)
        rough=create(unreal.MaterialExpressionTextureSampleParameter2D,"LeafRoughnessMap",-1160,370)
        rough.set_editor_property("texture",default_texture)
        particle=create(unreal.MaterialExpressionParticleColor,"",-1160,760)
        tint=create(unreal.MaterialExpressionMultiply,"",-560,-550)
        edge(albedo,"RGB",tint,"A")
        edge(particle,"RGB",tint,"B")
        port(tint,"",unreal.MaterialProperty.MP_BASE_COLOR)
        port(normal,"RGB",unreal.MaterialProperty.MP_NORMAL)
        multiplier=create(unreal.MaterialExpressionScalarParameter,"OpacityMultiplier",-710,145)
        multiplier.set_editor_property("default_value",1.0)
        opacity_rgba=create(unreal.MaterialExpressionMultiply,"",-340,155)
        edge(opacity,"R",opacity_rgba,"A")
        edge(multiplier,"",opacity_rgba,"B")
        particle_alpha=create(unreal.MaterialExpressionMultiply,"",-90,155)
        edge(opacity_rgba,"",particle_alpha,"A")
        edge(particle,"A",particle_alpha,"B")
        port(particle_alpha,"",unreal.MaterialProperty.MP_OPACITY_MASK)
        roughness_scale=create(unreal.MaterialExpressionScalarParameter,"RoughnessScale",-680,445)
        roughness_scale.set_editor_property("default_value",1.0)
        roughness_value=create(unreal.MaterialExpressionMultiply,"",-400,435)
        edge(rough,"R",roughness_value,"A")
        edge(roughness_scale,"",roughness_value,"B")
        port(roughness_value,"",unreal.MaterialProperty.MP_ROUGHNESS)
        spec=create(unreal.MaterialExpressionScalarParameter,"LeafSpecular",-160,560)
        spec.set_editor_property("default_value",.2)
        port(spec,"",unreal.MaterialProperty.MP_SPECULAR)
        metallic=create(unreal.MaterialExpressionConstant,"",-160,700)
        metallic.set_editor_property("r",0.0)
        port(metallic,"",unreal.MaterialProperty.MP_METALLIC)
        transmission=create(unreal.MaterialExpressionScalarParameter,"LeafTransmission",-535,-85)
        transmission.set_editor_property("default_value",.42)
        translucency=create(unreal.MaterialExpressionMultiply,"",-270,-150)
        edge(tint,"",translucency,"A")
        edge(transmission,"",translucency,"B")
        port(translucency,"",unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        compile_errors=LIB.recompile_material(material)
        if compile_errors and any("error" in str(x).lower() for x in compile_errors):
            raise RuntimeError("PBR母材质编译失败: "+str(compile_errors))
        if not ED.save_loaded_asset(material,only_if_is_dirty=False):
            raise RuntimeError("UE5.8实际保存母材质失败")
        if ED.load_asset(PATH) is None:
            raise RuntimeError("保存后重新读取母材质失败")
        OUTPUT["created"]=True
        OUTPUT["success"]=True
        OUTPUT["model"]=name
        OUTPUT["connections"]="PBR Color+Normal+OpacityMask+Roughness+Specular+Metallic0+Subsurface"
        print("AMBIENT_PBR_MASTER_SAVED",PATH,flush=True)
except Exception as exc:
    OUTPUT["errors"].append(str(exc))
    print("AMBIENT_PBR_MASTER_ERROR",str(exc),flush=True)
    raise
finally:
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(OUTPUT,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("AMBIENT_PBR_MASTER_REPORT",REPORT,OUTPUT["success"],flush=True)
