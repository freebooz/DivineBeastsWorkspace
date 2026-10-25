# -*- coding: utf-8 -*-
"""SSLM V2返工独立冷重载：只验证实际新网格/材质，以及V1图集切片配置。
不修改资产，不将类型/文件检查称为Niagara、性能或人工视觉通过。
"""
import unreal,json,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2'
OUT=ROOT/'Saved/Validation/OrbProjectileV2'
report={'engine':unreal.SystemLibrary.get_engine_version(),'status':'checking','V1HumanVerdict':'REJECTED','V2HumanVerdict':'NOT_SUBMITTED_FOR_FINAL_ACCEPTANCE','assets':[],'errors':[],'newNiagaraCreated':False,'realSpellIntegrated':False,'qualityPassed':False}
try:
    assert report['engine'].startswith('5.8')
    for name,cls in [('SM_DBA_FireCore_V2',unreal.StaticMesh),('SM_DBA_IceCrystal_V2',unreal.StaticMesh),('M_DBA_FireCoreFlow_V2',unreal.Material),('M_DBA_FireVeil_V2',unreal.Material),('M_DBA_IceCrystal_V2',unreal.Material),('M_DBA_ColdMist_V2',unreal.Material),('M_DBA_OrbTrail_V2',unreal.Material),('MI_DBA_FireTrail_V2',unreal.MaterialInstanceConstant),('MI_DBA_IceTrail_V2',unreal.MaterialInstanceConstant)]:
        path=BASE+('/Meshes/' if name.startswith('SM_') else '/Materials/')+name
        a=unreal.load_asset(path)
        assert isinstance(a,cls),'原生类型不符 '+path
        entry={'path':path,'actualClass':a.get_class().get_name()}
        if isinstance(a,unreal.StaticMesh):
            entry['bounds']=str(a.get_bounds())
            try:entry['lod0Vertices']=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_number_verts(a,0)
            except Exception as e:entry['vertexReadLimitation']=str(e)
        report['assets'].append(entry)
    legacy='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1/Niagara/Systems/NS_DBA_FireOrb_Form_Review'
    old=unreal.load_asset(legacy)
    typ=unreal.load_class(None,'/Script/Niagara.NiagaraSpriteRendererProperties')
    report['V1SpriteSubImageAudit']=[]
    for r in unreal.ObjectIterator(typ):
        if not r.get_path_name().startswith(old.get_path_name()+':'):continue
        row={'renderer':r.get_path_name()}
        for prop in ['SubImageSize','bSubImageBlend']:
            try:row[prop]=str(r.get_editor_property(prop))
            except Exception as e:row[prop+'Error']=str(e)
        report['V1SpriteSubImageAudit'].append(row)
    report['blockedNiagaraAuthorExists']=(ROOT/'Tools/Unreal/VFX/DevelopmentMagic/AuthorOrbV2Niagara.py').exists()
    report['status']='native_mesh_material_cold_load_verified_not_final_VFX'
except Exception:
    report['status']='failed';report['errors'].append(traceback.format_exc());raise
finally:
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'NativeColdCheck.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('ORBV2_NATIVE_CHECK',report['status'],'assets',len(report['assets']),'errors',len(report['errors']),flush=True)
