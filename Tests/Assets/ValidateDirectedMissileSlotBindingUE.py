# -*- coding: utf-8 -*-
"""独立UE进程只读重载技能槽和运行资源；不是联机/GPU/Cook验收。"""
import json,runpy
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileSlotBinding'
runpy.run_path(str(ROOT/'Tests/Assets/ValidateZodiacPlaceholderUE.py'),run_name='__main__')
base='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Runtime/DirectedMissile'
definition=unreal.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_DirectedMissileFlight')
assert isinstance(definition,unreal.GamePlatformVFXAttachedDefinition)
assert definition.niagara_system.get_path_name().startswith(base+'/Niagara/NS_DBA_DirectedMissile_Runtime.')
assert definition.max_lifetime_seconds>=2.2
reg=unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous([base],force_rescan=True)
assets=reg.get_assets_by_path(base,recursive=True)
assert len(assets)==12,len(assets)
loaded=[]
for data in assets:
    asset=data.get_asset();assert asset,data.package_name
    loaded.append(asset.get_path_name())
    deps=reg.get_dependencies(data.package_name,unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))
    assert not any('/Review/' in str(d) for d in deps),(data.package_name,deps)
for k in ('Core','Petals'):
    m=unreal.load_asset(base+'/Materials/M_DBA_MissileV1_'+k)
    assert m.blend_mode==unreal.BlendMode.BLEND_TRANSLUCENT
    unreal.MaterialEditingLibrary.recompile_material(m)
mesh=unreal.load_asset(base+'/Meshes/SM_DBA_DirectedMissile_V1')
assert len(mesh.static_materials)==3
assert all(str(s.material_interface.get_path_name()).startswith(base+'/Materials/') for s in mesh.static_materials)
result=dict(status='PASS',nativeRuntimePackages=len(assets)+1,slot1='DirectedMissile',slot5='Healing',heroCount=12,loaded=loaded,reviewReferences=False,serverHealingTestSeparate=True,networkVerified=False,cookVerified=False)
(OUT/'ColdBinding.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print('MISSILE_COLD_BINDING_PASS',len(assets)+1)
