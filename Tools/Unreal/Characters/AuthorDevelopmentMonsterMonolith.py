#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""原创开发石灵视觉作者；Monolith制作可编辑组件蓝图和三份项目材质。
这不是最终怪物美术/AI：轮廓为巨肩、石拳、玉石眼及胸口核心；所有碰撞/伤害在服务器原生怪物。
装饰Actor不复制、不生成伤害、不带碰撞，不修改地图、PlayerStart或已有资源。
inspect只列计划，apply仅首次创建；编译/保存失败立即终止，不删除或自动覆盖。
"""
import argparse
import importlib.util
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
DEST='/DBAContentPack_Common/Development/Monsters'
BP=DEST+'/BP_DBA_AncientStoneSpiritVisual'
# 各部件厘米尺寸来自本开发原型设计，球体/方体默认100厘米；非图片测量或正式资产比例。
PARTS=[('Body','Sphere',(0,0,98),(0.86,0.76,1.08),'Stone'),
       ('Chest','Cube',(20,0,116),(0.6,0.64,0.6),'EdgeStone'),
       ('Head','Cube',(4,0,175),(0.58,0.65,0.58),'Stone'),
       ('Jaw','Cube',(15,0,152),(0.48,0.58,0.2),'EdgeStone'),
       ('Core','Sphere',(53,0,117),(0.18,0.18,0.18),'JadeGlow'),
       ('EyeL','Sphere',(35,-18,180),(0.06,0.13,0.07),'JadeGlow'),
       ('EyeR','Sphere',(35,18,180),(0.06,0.13,0.07),'JadeGlow'),
       ('ShoulderL','Sphere',(0,-65,131),(0.72,0.72,0.72),'EdgeStone'),
       ('ShoulderR','Sphere',(0,65,131),(0.72,0.72,0.72),'EdgeStone'),
       ('ArmL','Cube',(0,-66,86),(0.35,0.38,0.68),'Stone'),
       ('ArmR','Cube',(0,66,86),(0.35,0.38,0.68),'Stone'),
       ('FistL','Sphere',(4,-65,52),(0.57,0.53,0.56),'EdgeStone'),
       ('FistR','Sphere',(4,65,52),(0.57,0.53,0.56),'EdgeStone'),
       ('LegL','Cube',(0,-22,40),(0.4,0.38,0.65),'Stone'),
       ('LegR','Cube',(0,22,40),(0.4,0.38,0.65),'Stone'),
       ('FootL','Cube',(14,-22,10),(0.66,0.44,0.2),'EdgeStone'),
       ('FootR','Cube',(14,22,10),(0.66,0.44,0.2),'EdgeStone')]


def author():
    """材质、纯表现蓝图、组件、碰撞禁用与真正编译依次执行；不保存权威或地图状态。"""
    spec=importlib.util.spec_from_file_location('casting_transport',ROOT/'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    t=importlib.util.module_from_spec(spec); spec.loader.exec_module(t); p=t.transport()
    p.editor_python("import unreal\nassert not unreal.EditorAssetLibrary.does_asset_exist('"+BP+"'),'石灵资产已存在，不能覆盖'")
    p.editor_python('''import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools(); lib=unreal.MaterialEditingLibrary
root='/DBAContentPack_Common/Development/Monsters/Materials'
for name,color,rough,emission in [('Stone',(0.075,0.12,0.10),0.35,None),('EdgeStone',(0.18,0.25,0.22),0.28,None),('JadeGlow',(0.03,0.35,0.15),0.16,(0.08,2.0,0.55))]:
    path=root+'/M_DBA_StoneSpirit'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path): raise RuntimeError('专用材质已存在，不覆盖: '+path)
    material=tools.create_asset('M_DBA_StoneSpirit'+name,root,unreal.Material,unreal.MaterialFactoryNew())
    def color_node(value):
        node=lib.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
        node.set_editor_property('constant',unreal.LinearColor(*value,1)); return node
    lib.connect_material_property(color_node(color),'',unreal.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,rough),(unreal.MaterialProperty.MP_METALLIC,0.12)]:
        node=lib.create_material_expression(material,unreal.MaterialExpressionConstant); node.set_editor_property('r',value); lib.connect_material_property(node,'',prop)
    if emission: lib.connect_material_property(color_node(emission),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material,False): raise RuntimeError('石灵材质保存失败')
    print('STONE_MATERIAL_SAVED',path)''')
    p.call('blueprint_query',{'action':'create_blueprint','params':{'save_path':BP,'parent_class':'Actor'}})
    p.call('blueprint_query',{'action':'add_component','params':{'asset_path':BP,'component_class':'SceneComponent','component_name':'StoneRoot'}})
    for name,mesh,location,scale,material in PARTS:
        p.call('blueprint_query',{'action':'add_component','params':{'asset_path':BP,'component_class':'StaticMeshComponent','component_name':name,'parent':'StoneRoot'}})
        def set_property(prop,value):
            return p.call('blueprint_query',{'action':'set_component_property','params':{'asset_path':BP,'component_name':name,'property_name':prop,'value':value}})
        set_property('StaticMesh',"StaticMesh'/Engine/BasicShapes/"+mesh+'.'+mesh+"'")
        set_property('RelativeLocation','(X=%s,Y=%s,Z=%s)'%location)
        set_property('RelativeScale3D','(X=%s,Y=%s,Z=%s)'%scale)
        set_property('OverrideMaterials',"(Material'"+DEST+'/Materials/M_DBA_StoneSpirit'+material+'.M_DBA_StoneSpirit'+material+"')")
        # BodyInstance保留引擎完整默认值，仅改碰撞启用字段；不靠可见模型代替权威胶囊。
        set_property('BodyInstance','(CollisionEnabled=NoCollision)')
        set_property('bGenerateOverlapEvents','False')
    # 世界空间标签也是Monolith保存的真实视觉控件；运行时只通过生命事件更新文案，不在Tick轮询。
    # Common仅保留中立世界文字槽；中文字体与正文由项目客户端从UI包/生命事实事件装配。
    for name,text,z,size in [('NameLabel','',236,15),('LifeLabel','',215,13)]:
        p.call('blueprint_query',{'action':'add_component','params':{'asset_path':BP,'component_class':'TextRenderComponent','component_name':name,'parent':'StoneRoot'}})
        properties={'Text':text,'WorldSize':str(size),'HorizontalAlignment':'EHTA_Center',
                    'RelativeLocation':f'(X=0,Y=0,Z={z})','TextRenderColor':'(R=180,G=238,B=205,A=255)',
                    'ComponentTags':'("DBA.Monster.'+name+'")'}
        for prop,value in properties.items():
            p.call('blueprint_query',{'action':'set_component_property','params':{'asset_path':BP,'component_name':name,'property_name':prop,'value':value}})
    report=p.call('blueprint_query',{'action':'compile_blueprint','params':{'asset_path':BP}})
    if report.get('error_count',1) or report.get('warning_count',1): raise RuntimeError('石灵蓝图编译不干净: '+json.dumps(report))
    p.call('blueprint_query',{'action':'save_asset','params':{'asset_path':BP}})
    verify_visual(p)
    (p.EVIDENCE/'MonsterVisual.json').write_text(json.dumps({'Blueprint':BP,'Components':len(PARTS),'Compile':report,'ReloadVerified':False,'ManualVisualAccepted':False},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'Blueprint':BP,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


def verify_visual(p):
    """使用Monolith真实父类查询，避免Python未公开的Blueprint.ParentClass属性造成假失败。"""
    parent=p.call('blueprint_query',{'action':'get_parent_class','params':{'asset_path':BP}})
    if parent.get('parent_class',{}).get('path')!='/Script/Engine.Actor': raise RuntimeError('石灵装饰不是纯Actor父类')
    p.editor_python("import unreal\nbp=unreal.load_asset('"+BP+"')\ncdo=unreal.get_default_object(bp.generated_class())\nassert isinstance(cdo,unreal.Actor) and not cdo.get_editor_property('replicates')\nprint('STONE_VISUAL_SAVED',bp.get_path_name())")
    (p.EVIDENCE/'MonsterVerified.json').write_text(json.dumps({'Blueprint':BP,'Parent':parent,'ReloadVerified':False,'ManualVisualAccepted':False},ensure_ascii=False,indent=2),encoding='utf-8')


def verify_existing():
    """仅核验已保存的本轮蓝图，不重建先前已成功资源；保持最初失败日志以便人工追溯。"""
    spec=importlib.util.spec_from_file_location('casting_transport',ROOT/'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    t=importlib.util.module_from_spec(spec); spec.loader.exec_module(t); p=t.transport()
    verify_visual(p)
    print(json.dumps({'Verified':True,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


def prepare_label_slots():
    """保护本轮BP并清除错误的ASCII中文默认值；不硬引UI包，随后由事件适配组合真实字体。"""
    spec=importlib.util.spec_from_file_location('casting_transport',ROOT/'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    t=importlib.util.module_from_spec(spec); spec.loader.exec_module(t); p=t.transport()
    p.editor_python("import unreal\nassert not any(x.get_name()=='"+BP+"' for x in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()),'石灵有未保存修改，不覆盖'")
    source=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/Content/Development/Monsters/BP_DBA_AncientStoneSpiritVisual.uasset'
    shutil.copy2(source,p.EVIDENCE/'Monster.before-label-slots.uasset')
    for name in ('NameLabel','LifeLabel'):
        p.call('blueprint_query',{'action':'set_component_property','params':{'asset_path':BP,'component_name':name,'property_name':'Text','value':''}})
    report=p.call('blueprint_query',{'action':'compile_blueprint','params':{'asset_path':BP}})
    if report.get('error_count',1) or report.get('warning_count',1): raise RuntimeError('文字槽编译失败')
    p.call('blueprint_query',{'action':'save_asset','params':{'asset_path':BP}})
    print(json.dumps({'NeutralLabelSlots':2,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--apply',action='store_true'); parser.add_argument('--verify',action='store_true'); parser.add_argument('--prepare-label-slots',action='store_true'); args=parser.parse_args()
    if args.apply: author()
    elif args.verify: verify_existing()
    elif args.prepare_label_slots: prepare_label_slots()
    else: print(json.dumps({'Blueprint':BP,'Components':len(PARTS),'Modified':False},ensure_ascii=False))
