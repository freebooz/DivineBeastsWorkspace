"""仅由当前UE5.8的Monolith运行：创建真实Profile/CurveFloat，原位绑定唯一火球Definition。"""
from pathlib import Path
import json
import uuid
import unreal as u

root=Path(u.Paths.project_dir()).resolve().parent
cfg=json.loads((root/'Tools/Unreal/Combat/ProjectileFlightProfiles.json').read_text('utf-8'))
out=root/'Saved/Validation/ProjectileMotion20261101'/('Native-'+str(uuid.uuid4()))
out.mkdir(parents=True)
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'先处理地图所有权，不保存外部地图'
before={p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
allowed={cfg['fireball_definition'],cfg['fireball_profile'],cfg['demo_directory']+'/C_DBA_ProjectileSpeed_EaseOut'}
allowed.update(cfg['demo_directory']+'/DA_DBA_ProjectileFlight_'+row['name'] for row in cfg['profiles'] if row['name']!='Fireball')
assert before<=allowed,'存在本次作者范围外的脏包，不能顺带覆盖人工资源'
tools=u.AssetToolsHelpers.get_asset_tools()
cls=u.load_class(None,'/Script/GamePlatformCombat.GamePlatformCombatProjectileFlightProfile')
assert cls,'必须先构建并冷启动包含新反射类型的Editor'
owned=[]

def asset(path,asset_class,factory):
    obj=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    if obj is None:
        folder,name=path.rsplit('/',1)
        obj=tools.create_asset(name,folder,asset_class,factory)
    assert obj and obj.get_class()==asset_class,path
    owned.append(obj)
    return obj

curve_path=cfg['demo_directory']+'/C_DBA_ProjectileSpeed_EaseOut'
curve=asset(curve_path,u.CurveFloat.static_class(),u.CurveFloatFactory())
# FloatCurve为受保护属性，由外层Monolith set_property_at_path写入，不绕过Python访问限制。
assert abs(curve.get_float_value(.5)-.75)<.001
rows=[]
for row in cfg['profiles']:
    path=cfg['fireball_profile'] if row['name']=='Fireball' else cfg['demo_directory']+'/DA_DBA_ProjectileFlight_'+row['name']
    factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',cls)
    profile=asset(path,cls,factory)
    flight=u.GamePlatformCombatProjectileFlightConfig()
    flight.set_editor_property('bEnabled',True)
    flight.set_editor_property('bEnableHoming',False)
    for name,value in row.items():
        if name=='name':continue
        if name=='speed_mode':value=getattr(u.GamePlatformProjectileSpeedMode,value)
        native=('b'+''.join(p.title() for p in name[2:].split('_'))) if name.startswith('b_') else ''.join(p.title() for p in name.split('_'))
        flight.set_editor_property(native,value)
    if row['name']=='Curve':flight.set_editor_property('speed_curve',curve)
    profile.set_editor_property('flight',flight)
    assert u.EditorAssetLibrary.save_loaded_asset(profile,False)
    rows.append({'path':path,'config':flight.export_text()})
assert u.EditorAssetLibrary.save_loaded_asset(curve,False)
definition=u.load_asset(cfg['fireball_definition']);assert definition
motion=definition.get_editor_property('projectile_motion')
motion.set_editor_property('flight_profile',u.load_asset(cfg['fireball_profile']))
definition.set_editor_property('projectile_motion',motion)
assert u.EditorAssetLibrary.save_loaded_asset(definition,False)
owned.append(definition)
report={'profiles':rows,'curve':curve_path,'definition':cfg['fireball_definition'],
        'saved':[a.get_path_name() for a in owned],'network_verified':False,'human_accepted':False}
(out/'Profiles.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8')
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
print(json.dumps({'status':'Saved','report':str(out/'Profiles.json'),'count':len(rows)},ensure_ascii=False))
