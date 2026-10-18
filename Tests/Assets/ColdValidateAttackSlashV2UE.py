# -*- coding: utf-8 -*-
"""独立UE命令行进程从磁盘只读冷加载弧刃审核包，并经Monolith编译门面核实CPU脚本。

不保存/修改任何原生资产；本检查不代表Cook、打包GPU或人工视觉通过。
"""
from pathlib import Path
import json,os,traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
OUT=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2/ColdValidation.json'
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash'
report=dict(passed=False,pid=os.getpid(),nativeReload=True,manualAccepted=False,packagedGPUVerified=False)
try:
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
    assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_AttackSlash_V2');assert isinstance(mesh,unreal.StaticMesh)
    slots=mesh.get_editor_property('static_materials');assert len(slots)==3
    report['materials']=[s.material_interface.get_path_name() for s in slots]
    assert len(set(report['materials']))==3 and all(p.startswith(BASE+'/Materials/') for p in report['materials'])
    textures=[]
    for suffix,srgb,compression in [('Color',True,unreal.TextureCompressionSettings.TC_DEFAULT),('Masks',False,unreal.TextureCompressionSettings.TC_MASKS),('Normal',False,unreal.TextureCompressionSettings.TC_NORMALMAP)]:
        t=unreal.load_asset(BASE+'/Textures/T_DBA_SlashV2_'+suffix);assert isinstance(t,unreal.Texture2D)
        assert t.get_editor_property('srgb')==srgb and t.get_editor_property('compression_settings')==compression
        assert t.blueprint_get_size_x()==2048 and t.blueprint_get_size_y()==512
        textures.append(dict(path=t.get_path_name(),sRGB=srgb,compression=str(compression)))
    report['textures']=textures
    system=unreal.load_asset(BASE+'/Niagara/NS_DBA_AttackSlash_V2');assert isinstance(system,unreal.NiagaraSystem)
    effect=system.get_editor_property('effect_type');assert effect and effect.get_path_name().startswith(BASE+'/Scalability/NET_DBA_Review_CombatCast.')
    raw,success,error=unreal.MonolithNiagaraQueryLibrary.get_niagara_stats(system.get_path_name(),True);assert success,error
    diagnostics=json.loads(raw);report['diagnostics']=diagnostics
    assert diagnostics['error_count']==0 and len(diagnostics['emitter_stats'])==1
    assert diagnostics['emitter_stats'][0]['sim_target']=='CPU'
    definition=unreal.load_asset(BASE+'/Definitions/DA_DBA_VFX_AttackSlashV2_Review');assert isinstance(definition,unreal.GamePlatformVFXInstantDefinition)
    assert definition.get_editor_property('niagara_system')==system
    schema=definition.get_editor_property('parameter_schema');rules=schema.get_editor_property('rules')
    report['parameterRules']=[dict(name=str(r.name),min=r.min_value,max=r.max_value) for r in rules];assert len(rules)==3
    assert definition.get_editor_property('require_fixed_bounds') and definition.get_editor_property('allow_pooling')
    review=unreal.load_asset(BASE+'/Review/L_DBA_Review_AttackSlashV2');assert isinstance(review,unreal.World)
    report.update(passed=True,reviewMap=review.get_path_name(),effectType=effect.get_path_name())
    print('ATTACK_SLASH_V2_COLD_VALIDATED',json.dumps(report,ensure_ascii=False))
except Exception:
    report['error']=traceback.format_exc();raise
finally:
    OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
