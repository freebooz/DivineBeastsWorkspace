# -*- coding: utf-8 -*-
"""独立命令行只读冷加载定向飞弹，锁定最终保存包及平台Projectile合同。"""
from pathlib import Path
import json,hashlib,os,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile';CONTENT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/CommonSkillVFX/Review/V1/DirectedMissile';OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1/ColdValidation.json'
def hashes():return {str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(CONTENT.rglob('*')) if p.suffix in ('.uasset','.umap')}
report=dict(passed=False,pid=os.getpid(),manualAccepted=False,packagedGPUVerified=False)
try:
    assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
    before=hashes();assert before
    mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_DirectedMissile_V1');assert isinstance(mesh,unreal.StaticMesh);slots=mesh.get_editor_property('static_materials');assert len(slots)==3
    report['materials']=[s.material_interface.get_path_name() for s in slots];assert len(set(report['materials']))==3
    for kind in ('Head','Trail'):
        t=unreal.load_asset(BASE+'/Textures/T_DBA_MissileV1_'+kind+'_Color');assert isinstance(t,unreal.Texture2D) and t.get_editor_property('srgb') and t.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_BC7
    air=unreal.load_asset(BASE+'/Materials/M_DBA_MissileV1_AirWave');assert air.get_editor_property('refraction_method')==unreal.RefractionMode.RM_2D_OFFSET
    system=unreal.load_asset(BASE+'/Niagara/NS_DBA_DirectedMissile_V1');assert isinstance(system,unreal.NiagaraSystem)
    result,success,error=unreal.MonolithNiagaraQueryLibrary.get_niagara_stats(system.get_path_name(),True);assert success,error
    d=json.loads(result);assert d['error_count']==0 and len(d['emitter_stats'])==4 and all(e['sim_target']=='CPU' for e in d['emitter_stats']);report['diagnostics']=d
    effect=system.get_editor_property('effect_type');assert 'CombatProjectile' in effect.get_name()
    definition=unreal.load_asset(BASE+'/Definitions/DA_DBA_VFX_DirectedMissileV1_Review');assert isinstance(definition,unreal.GamePlatformVFXProjectileDefinition) and definition.get_editor_property('niagara_system')==system
    assert definition.get_editor_property('default_parameters').get_editor_property('float_parameters')['User.TrailWidth']==26.
    assert definition.get_editor_property('require_fixed_bounds') and definition.get_editor_property('allow_pooling')
    review=unreal.load_asset(BASE+'/Review/L_DBA_Review_DirectedMissileV1');assert isinstance(review,unreal.World)
    after=hashes();assert before==after
    report.update(passed=True,nativePackageSHA256=after,packagesUnchangedDuringValidation=True,effectType=effect.get_path_name(),reviewMap=review.get_path_name(),productionBindingsChanged=False)
    print('DIRECTED_MISSILE_COLD_VALIDATED',len(after))
except Exception:report['error']=traceback.format_exc();raise
finally:OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
