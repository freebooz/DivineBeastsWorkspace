# -*- coding: utf-8 -*-
"""独立命令行进程从保存包冷加载凝锋V3，仅读取资产和强制编译CPU脚本，不保存资产。"""
from pathlib import Path
import hashlib,json,os,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V3/AttackSlash'
OUT=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3/ColdValidation.json'
CONTENT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/CommonSkillVFX/Review/V3/AttackSlash'
def package_hashes():
    return {str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(CONTENT.rglob('*')) if p.suffix in ('.uasset','.umap')}
report=dict(passed=False,pid=os.getpid(),manualAccepted=False,packagedGPUVerified=False)
try:
    before=package_hashes();assert len(before)==13
    assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
    mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_AttackSlash_V3');assert isinstance(mesh,unreal.StaticMesh)
    slots=mesh.get_editor_property('static_materials');assert len(slots)==4
    report['materials']=[s.material_interface.get_path_name() for s in slots];assert len(set(report['materials']))==4 and all(p.startswith(BASE+'/Materials/') for p in report['materials'])
    air=unreal.load_asset(BASE+'/Materials/M_DBA_SlashV3_AirWave');assert isinstance(air,unreal.Material)
    assert air.get_editor_property('refraction_method')==unreal.RefractionMode.RM_2D_OFFSET
    report['airWave']=dict(material=air.get_path_name(),refraction='bounded explicit 2D offset',lifecycle='same Niagara particle',packagedGPUVerified=False)
    t=unreal.load_asset(BASE+'/Textures/T_DBA_SlashV3_Energy_Color');assert isinstance(t,unreal.Texture2D) and t.get_editor_property('srgb')
    assert t.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_BC7
    assert t.get_editor_property('power_of_two_mode')==unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO and t.get_editor_property('max_texture_size')==2048
    report['texture']=dict(path=t.get_path_name(),sRGB=True,compression='BC7',maxTextureSize=2048,sizeReportedWithoutRHI=[t.blueprint_get_size_x(),t.blueprint_get_size_y()],sizeNote='NullRHI reports imported source dimensions; use LiveAssetReport for rendered mip dimensions',source=t.get_editor_property('asset_import_data').get_first_filename())
    system=unreal.load_asset(BASE+'/Niagara/NS_DBA_AttackSlash_V3');assert isinstance(system,unreal.NiagaraSystem)
    effect=system.get_editor_property('effect_type');assert effect and 'CombatCast' in effect.get_name()
    result,success,error=unreal.MonolithNiagaraQueryLibrary.get_niagara_stats(system.get_path_name(),True);assert success,error
    d=json.loads(result);report['diagnostics']=d;assert d['error_count']==0 and len(d['emitter_stats'])==1 and d['emitter_stats'][0]['sim_target']=='CPU'
    definition=unreal.load_asset(BASE+'/Definitions/DA_DBA_VFX_AttackSlashV3_Review');assert isinstance(definition,unreal.GamePlatformVFXInstantDefinition) and definition.get_editor_property('niagara_system')==system
    assert definition.get_editor_property('require_fixed_bounds') and definition.get_editor_property('allow_pooling')
    rules=definition.get_editor_property('parameter_schema').get_editor_property('rules');assert len(rules)==3
    defaults=definition.get_editor_property('default_parameters').get_editor_property('float_parameters');assert abs(defaults['User.Duration']-.85)<.001
    report['parameterRules']=[dict(name=str(r.name),min=r.min_value,max=r.max_value) for r in rules]
    review=unreal.load_asset(BASE+'/Review/L_DBA_Review_AttackSlashV3');assert isinstance(review,unreal.World)
    after=package_hashes();assert before==after,'冷验证期间原生包发生变动，必须重新验证最终保存版本'
    report.update(passed=True,reviewMap=review.get_path_name(),effectType=effect.get_path_name(),nativePackageSHA256=after,packagesUnchangedDuringValidation=True)
    print('ATTACK_SLASH_V3_COLD_VALIDATED',json.dumps(report,ensure_ascii=False))
except Exception:report['error']=traceback.format_exc();raise
finally:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
