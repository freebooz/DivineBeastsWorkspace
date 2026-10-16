#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通过当前独占Monolith编辑器，将用户冰箭原图与三阶段概念接入现有真实资产。

只修改IceArrow独占目录的材质、运动及阶段参数，保留系统与既有定义身份。
必须显式--apply及编辑器PID；每次请求留存独立证据，失败即停止，不重建已有系统。
原图RGB/Alpha直接导入，GPU运动不决定服务器伤害、治疗或命中。
"""
import argparse
import importlib.util
import json
from pathlib import Path

from AuthorIceArrowNiagara import DEST, MODULE, HLSL, SYSTEMS

ROOT = Path(__file__).resolve().parents[4]


def main():
    """在已核实工程/PID中串行导入、修改、编译和保存；发布/Cook由统一入口另做。"""
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--apply', action='store_true')
    args.add_argument('--editor-pid', type=int, required=True)
    args.add_argument('--repair-new-emitters', action='store_true', help='仅修复本配方新增且实测未生成粒子的两个发射器')
    options = args.parse_args()
    if not options.apply:
        print('READ_ONLY_PLAN: reference texture, colored rune, crystal contrast, three-stage motion')
        return
    latest = json.loads((ROOT/'Saved/Validation/IceArrowVisualFollowup/Latest.json').read_text(encoding='utf-8-sig'))
    if latest['PID'] != options.editor_pid:
        raise RuntimeError('编辑器PID与本轮所有权记录不一致')
    spec = importlib.util.spec_from_file_location('concept_protocol', ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(p)
    p.EVIDENCE = Path(latest['Directory'])/'ConceptAuthoring'
    p.editor_python(f"import unreal,os\nassert os.getpid()=={options.editor_pid}\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\nassert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()")
    # 材质使用原图RGB与独立Alpha；透明接触距离为1cm，避免旧12cm柔化吞掉贴地细线。
    p.editor_python('''import unreal,json
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
dest='/DBAPresentationPack_Core/VFX/IceArrow'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
texture_path=dest+'/Textures/T_DBA_IceArrow_GroundRuneReference'
texture=unreal.load_asset(texture_path)
if texture is None:
 task=unreal.AssetImportTask();task.filename=str(root/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow/T_DBA_IceArrow_GroundRuneReference.png');task.destination_path=dest+'/Textures';task.destination_name='T_DBA_IceArrow_GroundRuneReference';task.automated=True;task.save=False;tools.import_asset_tasks([task]);texture=unreal.load_asset(texture_path)
assert isinstance(texture,unreal.Texture2D)
texture.set_editor_property('srgb',True);texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT)
texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture,False)
path=dest+'/Materials/M_DBA_IceArrow_RuneColor'
material=unreal.load_asset(path)
if material is None:
 material=tools.create_asset('M_DBA_IceArrow_RuneColor',dest+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
# 此新材质仅由本配方拥有；重试清理自己未完成的图，既有四份母材质不清图。
lib.delete_all_material_expressions(material)
material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);material.set_editor_property('two_sided',True)
lib.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,True)
tex=lib.create_material_expression(material,unreal.MaterialExpressionTextureSampleParameter2D,-600,0)
tex.set_editor_property('parameter_name','RuneTexture');tex.set_editor_property('texture',texture);tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
color=lib.create_material_expression(material,unreal.MaterialExpressionParticleColor,-600,220)
alpha=lib.create_material_expression(material,unreal.MaterialExpressionMultiply,-300,180)
assert lib.connect_material_expressions(tex,'A',alpha,'A') and lib.connect_material_expressions(color,'A',alpha,'B')
depth=lib.create_material_expression(material,unreal.MaterialExpressionDepthFade,-80,180);depth.set_editor_property('fade_distance_default',1.0)
assert lib.connect_material_expressions(alpha,'',depth,'Opacity')
strength=lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter,-300,-120);strength.set_editor_property('parameter_name','RuneEmission');strength.set_editor_property('default_value',3.0)
emission=lib.create_material_expression(material,unreal.MaterialExpressionMultiply,-80,-120)
assert lib.connect_material_expressions(tex,'RGB',emission,'A') and lib.connect_material_expressions(strength,'',emission,'B')
assert lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(depth,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
for name in ['GroundOuter','GroundInner']:
 instance=unreal.load_asset(dest+'/Materials/MI_DBA_IceArrow_'+name);lib.set_material_instance_parent(instance,material)
 lib.set_material_instance_texture_parameter_value(instance,'RuneTexture',texture);lib.update_material_instance(instance)
 assert unreal.EditorAssetLibrary.save_loaded_asset(instance,False)
# 深蓝晶体保留明暗层次；裂纹只在高值发光，避免原噪声中间调把整个箭体烧成青白色块。
crystal=unreal.load_asset(dest+'/Materials/M_DBA_IceArrow_Crystal')
nodes=[o for o in unreal.ObjectIterator(unreal.MaterialExpression) if unreal.SystemLibrary.get_path_name(o).startswith(crystal.get_path_name()+':')]
for node in nodes:
 if isinstance(node,unreal.MaterialExpressionVectorParameter):
  name=str(node.get_editor_property('parameter_name'))
  if name=='IceTint':node.set_editor_property('default_value',unreal.LinearColor(.018,.075,.22,1))
  if name=='EdgeTint':node.set_editor_property('default_value',unreal.LinearColor(1.0,2.3,4.0,1))
 if isinstance(node,unreal.MaterialExpressionScalarParameter):
  name=str(node.get_editor_property('parameter_name'))
  if name=='EdgeIntensity':node.set_editor_property('default_value',2.2)
  if name=='CrystalOpacity':node.set_editor_property('default_value',.90)
 if isinstance(node,unreal.MaterialExpressionFresnel):node.set_editor_property('exponent',4.0)
edge=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionAdd) and n.get_name()=='MaterialExpressionAdd_0')
fracture=next((n for n in nodes if isinstance(n,unreal.MaterialExpressionCustom)),None)
if fracture is None:fracture=lib.create_material_expression(crystal,unreal.MaterialExpressionCustom,-550,580)
fracture.set_editor_property('description','连续冰晶裂隙，九邻域Voronoi边界，纯材质外观')
fracture.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
uv_input=unreal.CustomInput();uv_input.set_editor_property('input_name',unreal.Name('UV'));fracture.set_editor_property('inputs',[uv_input])
fracture.set_editor_property('code','float2 p=UV*float2(5,11);float2 cell=floor(p);float2 f=frac(p);float d1=10,d2=10;for(int y=-1;y<=1;y++){for(int x=-1;x<=1;x++){float2 n=float2(x,y);float2 k=cell+n;float2 h=frac(sin(float2(dot(k,float2(127.1,311.7)),dot(k,float2(269.5,183.3))))*43758.5453);float d=length(n+0.18+h*0.64-f);if(d<d1){d2=d1;d1=d;}else{d2=min(d2,d);}}}return 1-smoothstep(0.012,0.045,d2-d1);')
uv=next((n for n in nodes if isinstance(n,unreal.MaterialExpressionTextureCoordinate)),None)
if uv is None:uv=lib.create_material_expression(crystal,unreal.MaterialExpressionTextureCoordinate,-820,580)
assert lib.connect_material_expressions(uv,'',fracture,'UV') and lib.connect_material_expressions(fracture,'',edge,'A')
lib.recompile_material(crystal);assert unreal.EditorAssetLibrary.save_loaded_asset(crystal,False)
print('CONCEPT_MATERIALS_NATIVE_SAVED')
''')
    p.niagara('set_custom_hlsl_text', script_path=MODULE, hlsl=HLSL)
    # 用户改为仅地面法阵，不再由历史精修入口补回顶部或飞行中的空中法阵。
    for stage, emitters in SYSTEMS.items():
        path = DEST+'/Niagara/NS_DBA_IceArrow_'+stage
        operations = []
        for emitter, mode, count, life, material, mesh in emitters:
            raw = p.niagara('get_ordered_modules', asset_path=path, emitter=emitter)
            modules = json.loads(raw['result']) if isinstance(raw, dict) and isinstance(raw.get('result'), str) else raw
            motion = [n for n in modules if n.get('script_path', '').split('.')[0] == MODULE]
            if len(motion) not in (1,2):
                raise RuntimeError('已有调用合同变化，停止：'+stage+'/'+emitter)
            for node in motion:
                operations.append(dict(op='set_module_input_value', emitter=emitter, module_node=node['node_guid'], input='Mode', value=str(mode)))
                operations.append(dict(op='set_module_input_value', emitter=emitter, module_node=node['node_guid'], input='Life', value=str(life)))
            spawn = next(n for n in modules if 'SpawnBurst_Instantaneous' in n.get('script_path', ''))
            operations.append(dict(op='set_module_input_value', emitter=emitter, module_node=spawn['node_guid'], input='Spawn Count', value=str(count)))
            operations.append(dict(op='set_module_input_value', emitter=emitter, module_node='InitializeParticle', input='Lifetime', value=str(life)))
            if emitter == 'RuneRing':
                p.niagara('set_renderer_mesh', asset_path=path, emitter=emitter, renderer_index=0, mesh='/Engine/BasicShapes/Plane')
                p.niagara('set_renderer_material', asset_path=path, emitter=emitter, renderer_index=0, material=DEST+'/Materials/MI_DBA_IceArrow_GroundOuter')
            p.niagara('set_fixed_bounds', asset_path=path, emitter=emitter, min=[-650]*3, max=[650]*3, enabled=True)
        p.niagara('batch_execute', asset_path=path, operations=operations)
        p.niagara('set_fixed_bounds', asset_path=path, min=[-700]*3, max=[700]*3, enabled=True)
        p.niagara('request_compile', asset_path=path, force=True, synchronous=True)
        result = p.niagara('get_system_diagnostics', asset_path=path, compile_first=False)
        if result['error_count'] or result['warning_count']:
            raise RuntimeError('概念图系统编译失败：'+json.dumps(result, ensure_ascii=False))
        p.niagara('save_system', asset_path=path, only_if_dirty=False)
        print('CONCEPT_STAGE_SAVED', stage, len(emitters), flush=True)
    p.editor_python("import unreal\nassert unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset('"+MODULE+"'),False)\nprint('CONCEPT_MODULE_SAVED')")


if __name__ == '__main__':
    main()
