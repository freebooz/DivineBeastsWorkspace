"""Monolith原生接入已保留的V3：两个双掌系统和两个飞行系统，稳定Definition身份不变。
不创建第二套播放器，不变更命中资源、技能数值或权威弹道。先audit，再备份/apply，独立进程audit。
"""
from pathlib import Path
import hashlib
import json
import shutil
import unreal as u

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'Saved/Validation/FireIceV3Binding20261027'
PACK = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'
BASE = '/DBAPresentationPack_Core/VFX/'
PAIRS = {
    BASE+'DevelopmentMagic/Casting/Definitions/DA_DBA_VFX_DevFireballCast': BASE+'OrbProjectile/Review/V3/PalmRedesign/Niagara/NS_DBA_FirePalmCast_V3_ConceptReview',
    BASE+'DevelopmentMagic/Casting/Definitions/DA_DBA_VFX_DevFrostboltCast': BASE+'OrbProjectile/Review/V3/PalmRedesign/Niagara/NS_DBA_IcePalmCast_V3_ConceptReview',
    BASE+'DevelopmentMagic/Definitions/DA_DBA_VFX_DevFireballFlight': BASE+'OrbProjectile/Review/V3/CastFlight/Niagara/NS_DBA_FireOrbFlight_V3',
    BASE+'DevelopmentMagic/Definitions/DA_DBA_VFX_DevFrostboltFlight': BASE+'OrbProjectile/Review/V3/CastFlight/Niagara/NS_DBA_IceCrystalFlight_V3',
}
PARAMS = {'User.CastDurationSec': (.001, 600.), 'User.ElapsedAtStartSec': (0., 600.), 'User.HandSideSign': (-1., 1.)}

def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')

def audit(name='VFXAudit.json'):
    assert Path(u.Paths.get_project_file_path()).resolve() == ROOT/'Game/DivineBeastsArena.uproject'
    rows=[];errors=[]
    for path, expected in PAIRS.items():
        definition=u.load_asset(path);system=u.load_asset(expected)
        assert definition and isinstance(system,u.NiagaraSystem),path
        actual=definition.get_editor_property('niagara_system')
        actual=actual.get_path_name().split('.')[0] if actual else ''
        schema=definition.get_editor_property('parameter_schema')
        rules={str(r.name):r for r in schema.rules}
        if actual!=expected: errors.append(path+': system='+actual)
        if path.endswith('Cast'):
            for key,limits in PARAMS.items():
                if key not in rules or rules[key].type!=u.GamePlatformVFXParameterType.FLOAT or rules[key].min_value>limits[0]+.00001 or rules[key].max_value<limits[1]:
                    errors.append(path+': missing/range-invalid '+key)
        assert not definition.get_editor_property('quality_variants') and not definition.get_editor_property('platform_variants'), '存在覆盖路径，禁止默默忽略'
        rows.append({'definition':path,'system':actual,'expected':expected,'rules':schema.export_text()})
    report={'Passed':not errors,'Errors':errors,'Definitions':rows,'Project':u.Paths.get_project_file_path()}
    write(name,report);print('FIRE_ICE_V3_AUDIT',report['Passed'],len(errors));return report

def apply():
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    before=audit('VFXBefore.json')
    if before['Passed']:return before
    for path,target in PAIRS.items():
        source=PACK/(path.removeprefix('/DBAPresentationPack_Core/')+'.uasset')
        backup=OUT/'Backup'/source.relative_to(ROOT);backup.parent.mkdir(parents=True,exist_ok=True)
        if backup.exists():
            assert backup.read_bytes()==source.read_bytes(),'已有备份与源资产不同，停止重试'
        else:shutil.copy2(source,backup)
        definition=u.load_asset(path);system=u.load_asset(target)
        definition.set_editor_property('niagara_system',system)
        if path.endswith('Cast'):
            rules=[u.GamePlatformVFXParameterRule(name=key,type=u.GamePlatformVFXParameterType.FLOAT,
                   min_value=low,max_value=high,required=True) for key,(low,high) in PARAMS.items()]
            definition.set_editor_property('parameter_schema',u.GamePlatformVFXParameterSchema(rules=rules))
        assert u.EditorAssetLibrary.save_loaded_asset(definition,False),path
    result=audit('VFXGreen.json');assert result['Passed'],result['Errors']
    # 固定本次四个Definition与V3全部资源指纹，防止Cook期间输入变化；非运行地图不因此强制Cook。
    files={PACK/(p.removeprefix('/DBAPresentationPack_Core/')+'.uasset') for p in PAIRS}
    files.update((PACK/'VFX/OrbProjectile/Review/V3').rglob('*.uasset'))
    write('VFXSavedAssets.json',{'Passed':True,'Files':[{'Path':p.relative_to(ROOT).as_posix(),'SHA256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)]})
    return result
