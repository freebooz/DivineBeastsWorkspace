#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3审核Landscape真实图层权重/材质默认值只读诊断（UE5.8）。

确认十层Production母材质是否有零权重回退、正式V3QA地图是否确实拥有
LayerInfo（图层信息）和PaintedWeightmap（已绘制权重纹理），以定位黑帧原因。
不修改或保存任何资产。UE反射字段未公开时记录unknown而不是伪造零权重。
"""
from __future__ import annotations
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MAP=os.getenv("SSLM_TERRAIN_WEIGHT_AUDIT_MAP",
              "/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review")
REPORT=ROOT/"Game/Saved/Validation/TerrainV3WeightmapAudit_20261018"/(
    MAP.rsplit("/",1)[-1]+"_PaintWeightsAtLocation.json"
)

def attr(obj,key):
    try:return obj.get_editor_property(key)
    except Exception as exc:return "unavailable:"+str(exc)[:80]

def main():
    import unreal # type: ignore
    if not str(unreal.Paths.get_project_file_path()).replace("\\","/").endswith(
       "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE项目身份不正确")
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor or not editor.load_level(MAP):
        raise RuntimeError("真实Landscape审核地图不可加载："+MAP)
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    landscapes=[actor for actor in actors if isinstance(actor,unreal.LandscapeProxy)]
    if len(landscapes)!=1:
        raise RuntimeError("预期只有一张Landscape，实际："+str(len(landscapes)))
    terrain=landscapes[0]
    mat=attr(terrain,"landscape_material")
    layer_info=attr(terrain,"layer_infos")
    components=attr(terrain,"landscape_components")
    # ALandscape.LandscapeComponents字段UE5.8 Python可能不向EditorProperty反射。
    # 其次通过AActor.get_components_by_class访问官方UActorComponent集合；
    # 只要是真实LandscapeComponent即可计数，不把反射缺失误报成权重0。
    if isinstance(components,str):
        cls=getattr(unreal,"LandscapeComponent",None)
        if cls is not None:
            try:components=terrain.get_components_by_class(cls)
            except Exception as exc:components="unavailable:"+str(exc)[:150]
    comp_results=[]
    if not isinstance(components,str) and components:
        for idx,component in enumerate(components):
            if idx>=6:break
            row={"component":str(component.get_name())}
            for key in ("weightmap_textures","weightmap_layer_allocations",
                        "weightmap_usage_map","heightmap_texture"):
                data=attr(component,key)
                if isinstance(data,str):row[key]=data
                elif data is not None:
                    try:
                        row[key+"_count"]=len(data)
                        row[key+"_first"]=[str(x)[:250] for x in list(data)[:10]]
                    except Exception:row[key]=str(data)[:120]
            # UE5.8的官方LandscapeComponent Editor API可按坐标/层名读取权重。
            # 该API读取失败返回0，故所有0只能标记为“可疑”，不得推断真资产不存在。
            try:
                # UE5.8 Python Component未暴露get_component_bounds，原生SceneComponent
                # get_world_location是正式反射接口。1008m校准地形8x8组件，
                # 取该组件近中心位置（边长约126m）采样，避免恰在权重纹理拼接边界。
                loc=component.get_world_location()
                loc=unreal.Vector(loc.x+6300.0,loc.y+6300.0,loc.z)
                row["sampleLocation"]=[loc.x,loc.y,loc.z]
                layer_names=("LS_Grass","LS_ForestSoil","LS_Dirt","LS_Rock","LS_Scree",
                             "LS_Sand","LS_Gravel","LS_RoadDirt","LS_FarmSoil","LS_ShoreMud")
                row["sampleWeights"]={
                    name:round(float(component.editor_get_paint_layer_weight_by_name_at_location(
                        loc,unreal.Name(name))),5) for name in layer_names
                }
                row["weightSumAtComponentCenter"]=round(sum(row["sampleWeights"].values()),5)
            except Exception as exc:
                row["weightSampleError"]=str(exc)[:180]
            for attrname in ("visible","hidden_in_game","visible_in_scene_capture_only",
                             "use_as_occluder","cast_shadow"):
                row[attrname]=str(attr(component,attrname))[:140]
            try:
                applied=component.get_material(0)
                row["runtimeMaterial0"]=applied.get_path_name() if applied else None
            except Exception as exc:row["runtimeMaterialError"]=str(exc)[:140]
            comp_results.append(row)
    edit=unreal.MaterialEditingLibrary
    chain=[]
    asset=unreal.EditorAssetLibrary
    # V3 MI通过其Parent真实加载UMaterial，不能把实例当成母材质遍历。
    actual_mat=mat
    if isinstance(mat,unreal.MaterialInstanceConstant):
        actual_mat=mat.get_editor_property("parent")
    if isinstance(actual_mat,unreal.Material):
        expr=edit.get_material_expressions(actual_mat)
        for e in expr:
            if isinstance(e,unreal.MaterialExpressionLandscapeLayerSample):
                chain.append({"name":e.get_name(),
                              "parameter":str(attr(e,"parameter_name")),
                              "previewWeight":str(attr(e,"preview_weight"))})
        graph_count=len(expr)
        roots={}
        for name,prop in (("BaseColor",unreal.MaterialProperty.MP_BASE_COLOR),
                          ("Roughness",unreal.MaterialProperty.MP_ROUGHNESS),
                          ("Normal",unreal.MaterialProperty.MP_NORMAL)):
            x=edit.get_material_property_input_node(actual_mat,prop)
            roots[name]=x.get_name() if x else None
    else:
        graph_count=None
        roots={}
    actor_state={}
    for field in ("hidden","hidden_editor_views","is_temporarily_hidden_in_editor","is_hidden_ed",
                  "landscape_guid","landscape_material","use_dynamic_material_instance"):
        v=attr(terrain,field)
        actor_state[field]=str(v)[:220]
    root_component=attr(terrain,"root_component")
    if root_component is not None and not isinstance(root_component,str):
        for field in ("visible","hidden_in_game","mobility"):
            actor_state["root_"+field]=str(attr(root_component,field))[:200]
    report={
        "map":MAP,
        "actors":len(actors),
        "landscape":terrain.get_path_name(),
        "material":mat.get_path_name() if hasattr(mat,"get_path_name") else str(mat),
        "sourceMaterial":actual_mat.get_path_name() if hasattr(actual_mat,"get_path_name") else None,
        "expressionCount":graph_count,
        "landscapeLayerSamples":chain,
        "landscapeInfos":str(layer_info)[:400],
        "component_count":len(components) if not isinstance(components,str) and components is not None else str(components),
        "components":comp_results,
        "materialRoots":roots,
        "actorVisibilityAndRootState":actor_state,
        "reflectionSampleComponentProperties":sorted([n for n in dir(components[0]) if any(k in n.lower() for k in ("weight","heightmap","visibility","layer"))])[:100] if not isinstance(components,str) and components else [],
        "read_only":True,
    }
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_TERRAIN_V3_WEIGHTMAP_AUDIT",json.dumps(report,ensure_ascii=False)[:15500],flush=True)

if __name__=="__main__":main()
