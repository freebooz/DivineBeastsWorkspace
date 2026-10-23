# -*- coding: utf-8 -*-
"""Monolith原生制作：保留用户地图、原草地及唯一天气桥，只更新雪PBR支路。"""
import importlib.util
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parents[1]
if ROOT.name!='DivineBeastsWorkspace':raise RuntimeError('工程身份不匹配')
SOURCE=ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/SourceArt/Snow/PolyHavenSnow02'
DEST='/GamePlatformSurface/Textures/Snow/PolyHavenSnow02'
maps=[('Albedo','snow_02_diff_2k.png',True,unreal.TextureCompressionSettings.TC_BC7),
      ('Normal','snow_02_nor_dx_2k.png',False,unreal.TextureCompressionSettings.TC_NORMALMAP),
      ('ORM','snow_02_arm_2k.png',False,unreal.TextureCompressionSettings.TC_MASKS),
      ('Height','snow_02_disp_2k.png',False,unreal.TextureCompressionSettings.TC_GRAYSCALE)]
for semantic,file,srgb,compression in maps:
    name='T_GP_Snow02_'+semantic;path=DEST+'/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task=unreal.AssetImportTask();task.filename=str(SOURCE/file);task.destination_path=DEST
        task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex=unreal.load_asset(path)
    if not isinstance(tex,unreal.Texture2D):raise RuntimeError('扫描雪纹理导入失败: '+path)
    tex.set_editor_property('srgb',srgb);tex.set_editor_property('compression_settings',compression)
    tex.set_editor_property('never_stream',False)
    if semantic=='Normal':tex.set_editor_property('flip_green_channel',False)
    unreal.EditorAssetLibrary.save_loaded_asset(tex,only_if_is_dirty=False)

PATH='/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
mat=unreal.load_asset(PATH);lib=unreal.MaterialEditingLibrary
nodes={n.get_name():n for n in lib.get_material_expressions(mat)}
params={str(n.get_editor_property('parameter_name')):n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionScalarParameter)}
if 'VillageSnowParallaxHeightRatio' in params:
    raise RuntimeError('真实PBR支路已存在，请审查后增量调整，不重复创建')
spec=importlib.util.spec_from_file_location('snow_pbr',ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Scripts/SnowPBRDetail.py')
pbr=importlib.util.module_from_spec(spec);spec.loader.exec_module(pbr)
params['VillageSnowDetailScale'].set_editor_property('default_value',.005) # 同一扫描真实宽度200cm。
params['VillageSnowNormalStrength'].set_editor_property('default_value',.90)
params['VillageSnowAlbedoDetailContrast'].set_editor_property('default_value',1.)
params['VillageSnowEdgeSoftness'].set_editor_property('default_value',.08)
for n in nodes.values():
    if isinstance(n,unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='VillageSnowTint':
        n.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
albedo=nodes['MaterialExpressionTextureSampleParameter2D_1'];normal=nodes['MaterialExpressionTextureSampleParameter2D_2']
fallback=nodes['MaterialExpressionTextureSampleParameter2D_0'].get_editor_property('texture')
uv=nodes['MaterialExpressionMultiply_1'];position=nodes['MaterialExpressionWorldPosition_0']
coverage=nodes['MaterialExpressionSaturate_3']
# 向上31度以内完整留雪，31到69度连续减弱，垂直面不挂雪；消除缓坡持续透绿。
def scalar(name,value,x,y):
    n=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,x,y)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def wire(a,b,pin,out=''):
    linked=lib.connect_material_expressions(a,out,b,pin)
    if not linked and pin=='Input':linked=lib.connect_material_expressions(a,out,b,'')
    if not linked:raise RuntimeError('方向积雪接线失败: '+pin)
start=scalar('VillageSnowSlopeStartZ',.35,-2200,-3800)
span=scalar('VillageSnowSlopeRangeZ',.50,-2200,-3650)
sub=lib.create_material_expression(mat,unreal.MaterialExpressionSubtract,-1920,-3800)
wire(nodes['MaterialExpressionDotProduct_0'],sub,'A');wire(start,sub,'B')
divide=lib.create_material_expression(mat,unreal.MaterialExpressionDivide,-1640,-3800)
wire(sub,divide,'A');wire(span,divide,'B')
slope=lib.create_material_expression(mat,unreal.MaterialExpressionSaturate,-1360,-3800)
wire(divide,slope,'Input');wire(slope,nodes['MaterialExpressionMultiply_4'],'B')
result=pbr.build_detail(mat,uv,position,coverage,albedo,normal,fallback,'VillageSnow')
if not lib.connect_material_expressions(result['roughness'],'',nodes['MaterialExpressionLinearInterpolate_3'],'B'):
    raise RuntimeError('最终粗糙度支路接线失败')
# 旧雪AO节点用扫描AO替代固定1，底层原草地AO仍保留。
ao_mix=nodes['MaterialExpressionLinearInterpolate_6']
if not lib.connect_material_expressions(result['ao'],'',ao_mix,'B'):raise RuntimeError('雪AO接线失败')
for output,key in [(unreal.MaterialProperty.MP_SUBSURFACE_COLOR,'subsurface'),(unreal.MaterialProperty.MP_OPACITY,'opacity')]:
    if not lib.connect_material_property(result[key],'',output):raise RuntimeError('雪SSS输出接线失败')
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_SUBSURFACE)
messages=[str(v) for v in lib.recompile_material(mat)]
if any('error' in v.lower() for v in messages):raise RuntimeError('雪Shader编译失败: '+str(messages))
if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False):raise RuntimeError('真实雪母材质保存失败')
print('VILLAGE_SNOW_REALISM_AUTHORED',lib.get_num_material_expressions(mat),'nodes; 4 paired 2K scans; map untouched')
