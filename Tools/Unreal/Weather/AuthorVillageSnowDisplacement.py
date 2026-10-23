# -*- coding: utf-8 -*-
"""Monolith原生Nanite地形置换。仅生成显示数据，保护地形源高度、碰撞和出生点。

Displacement Multiplier默认5，明确采用世界厘米；Landscape缩放100时以0.1对象单位
提供10cm有界范围，再乘参数/10。原UE4截图的强度不能直接作为Landscape对象单位。
"""
import json,hashlib
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parents[1]
OUT=ROOT/'Saved/Validation/VillageSnowDisplacement20261023'
OUT.mkdir(parents=True,exist_ok=True)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if world.get_path_name()!='/DBAWorldPack_Village/Maps/L_Village_Start.L_Village_Start':raise RuntimeError('拒绝保存非新手村地图')
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():raise RuntimeError('存在人工未保存地图，先保护其编辑状态')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
landscapes=[a for a in actors if isinstance(a,unreal.LandscapeProxy)]
if len(landscapes)!=1:raise RuntimeError('地形数量发生变化，请先检查')
landscape=landscapes[0]
scale=landscape.get_actor_scale3d()
if abs(scale.x-100)>.001 or abs(scale.y-100)>.001 or abs(scale.z-100)>.001:raise RuntimeError('地形实际缩放与cm转换合同不符')
def snapshot():
    rows=[]
    for a in actors:
        if isinstance(a,(unreal.PlayerStart,unreal.LandscapeProxy)):
            # Python结构体字符串含临时内存地址；只比较真实数值，避免保存后的地址变化误报。
            p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
            rows.append({'name':a.get_name(),'location':[p.x,p.y,p.z],
                         'rotation':[r.pitch,r.yaw,r.roll],'scale':[s.x,s.y,s.z]})
    return rows
before=snapshot()
def height_sources(phase):
    # 源高度/权重为地图内嵌Texture2D；通过公开UObject迭代器取证，不绕过受保护组件属性。
    textures={t.get_path_name():t for t in unreal.ObjectIterator(unreal.Texture2D)
              if t.get_path_name().startswith('/DBAWorldPack_Village/Maps/L_Village_Start.')}
    if not textures:raise RuntimeError('未发现地图内嵌源纹理，拒绝无依据保存')
    result={}
    for index,(path,texture) in enumerate(sorted(textures.items())):
        filename=OUT/(phase+'_Heightmap_'+str(index)+'.tga')
        task=unreal.AssetExportTask();task.object=texture;task.exporter=unreal.TextureExporterTGA()
        task.filename=str(filename);task.automated=True;task.prompt=False
        if not unreal.Exporter.run_asset_export_task(task):raise RuntimeError('地形源高度取证失败')
        result[path]=hashlib.sha256(filename.read_bytes()).hexdigest()
    return result
heights_before=height_sources('Before')
(OUT/'HeightSources.before.json').write_text(json.dumps(heights_before,indent=2),encoding='utf-8')
(OUT/'ActorTransforms.before.json').write_text(json.dumps(before,indent=2),encoding='utf-8')
mat=unreal.load_asset('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
lib=unreal.MaterialEditingLibrary
nodes={n.get_name():n for n in lib.get_material_expressions(mat)}
params={str(n.get_editor_property('parameter_name')):n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionScalarParameter)}
height=next(n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='VillageSnowHeightTexture')
noisy=next((n for n in nodes.values() if n.get_editor_property('desc')=='GP.Snow.NoisyHeight'),None)
def node(cls,x,y):return lib.create_material_expression(mat,cls,x,y)
def wire(a,b,pin,out=''):
    linked=lib.connect_material_expressions(a,out,b,pin)
    if not linked and pin=='Input':linked=lib.connect_material_expressions(a,out,b,'')
    if not linked:raise RuntimeError('置换接线失败: '+pin)
if 'Displacement Multiplier' in params:
    scaled=next(n for n in nodes.values() if n.get_editor_property('desc')=='GP.Snow.Displacement.Output')
    params['Displacement Multiplier'].set_editor_property('default_value',5.)
    if noisy:wire(noisy,nodes['MaterialExpressionMultiply_14'],'A')
else:
    p=node(unreal.MaterialExpressionScalarParameter,-2300,5850)
    p.set_editor_property('parameter_name','Displacement Multiplier');p.set_editor_property('default_value',5.)
    p.set_editor_property('slider_min',0.);p.set_editor_property('slider_max',10.)
    clamp=node(unreal.MaterialExpressionClamp,-2000,5850)
    clamp.set_editor_property('min_default',0.);clamp.set_editor_property('max_default',10.);wire(p,clamp,'Input')
    divide=node(unreal.MaterialExpressionDivide,-1760,5850);divide.set_editor_property('const_b',10.);wire(clamp,divide,'A')
    h=node(unreal.MaterialExpressionMultiply,-1500,5670)
    wire(noisy or height,h,'A','' if noisy else 'R');wire(nodes['MaterialExpressionSaturate_3'],h,'B')
    scaled=node(unreal.MaterialExpressionMultiply,-1220,5670);wire(h,scaled,'A');wire(divide,scaled,'B')
    scaled.set_editor_property('desc','GP.Snow.Displacement.Output')
if not unreal.GamePlatformSurfaceMaterialAuthoringLibrary.connect_displacement(scaled):raise RuntimeError('未连接真实Displacement输出')
mat.set_editor_property('enable_tessellation',True)
scaling=unreal.DisplacementScaling();scaling.set_editor_property('magnitude',.1);scaling.set_editor_property('center',0.)
mat.set_editor_property('displacement_scaling',scaling)
fade=unreal.DisplacementFadeRange();fade.set_editor_property('start_size_pixels',8.);fade.set_editor_property('end_size_pixels',2.)
mat.set_editor_property('enable_displacement_fade',True);mat.set_editor_property('displacement_fade_range',fade)
lib.recompile_material(mat)
if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError('置换材质保存失败')
landscape.set_editor_property('enable_nanite',True)
# 引擎PreSave只由自身UpdateNanite生成并等待Nanite数据；不进行地形雕刻/重导入。
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('新手村Nanite数据保存失败')
after=snapshot()
if before!=after:raise RuntimeError('原Actor Transform改变，停止交付并检查')
heights_after=height_sources('After')
if heights_before!=heights_after:raise RuntimeError('地形源高度发生变化，停止交付并检查')
(OUT/'HeightSources.after.json').write_text(json.dumps(heights_after,indent=2),encoding='utf-8')
(OUT/'ActorTransforms.after.json').write_text(json.dumps(after,indent=2),encoding='utf-8')
components=landscape.get_components_by_class(unreal.LandscapeNaniteComponent)
print('VILLAGE_NANITE_DISPLACEMENT_AUTHORED multiplier=5 world_cm=0..5 scale=100 nanite_components=',len(components),'transforms_preserved=',before==after)
