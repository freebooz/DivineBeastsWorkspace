#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE编辑器内创建正式鸟群WorldDefinition数据资产，不定义第二套资源身份和加载器。
物种=选择不同Definition；数量及半径由现有Schema进行整数/浮点范围校验。
此脚本由Monolith run_python执行，创建的真实uasset可独立保存、重载与Cook。
"""
import unreal
ROOT='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
DEST=ROOT+'/Definitions'
EFFECT='/GamePlatformVFX/Environment/BirdFlock/EffectTypes/NET_GP_BirdFlock_Ambient'
SPECIES={'Seagull':(32,1700,850,250,0),'Swallow':(72,1150,570,340,2),'Eagle':(5,2400,1400,350,0)}
a=unreal.EditorAssetLibrary
t=unreal.AssetToolsHelpers.get_asset_tools()
assert 'DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path()
assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
fx=a.load_asset(EFFECT)
assert isinstance(fx,unreal.NiagaraEffectType)
a.make_directory(DEST)
factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.GamePlatformVFXWorldDefinition)
for species,(count,radius,height,speed,mode) in SPECIES.items():
    path=DEST+'/DA_DBA_VFX_BirdFlock_'+species
    if a.does_asset_exist(path):
        d=a.load_asset(path)
        assert isinstance(d,unreal.GamePlatformVFXWorldDefinition)
        print('BIRD_DEFINITION_EXISTS',path)
        continue
    system=a.load_asset(ROOT+'/Niagara/NS_DBA_BirdFlock_'+species)
    assert isinstance(system,unreal.NiagaraSystem)
    d=t.create_asset('DA_DBA_VFX_BirdFlock_'+species,DEST,unreal.GamePlatformVFXWorldDefinition,factory)
    assert d
    identity=unreal.GamePlatformId();identity.set_editor_property('namespace','presentation.dba.environment.birdflock');identity.set_editor_property('name',species.lower());identity.set_editor_property('logical_version',1)
    d.set_editor_property('logical_id',identity)
    d.set_editor_property('data_version',unreal.GamePlatformDataVersion())
    d.set_editor_property('niagara_system',system);d.set_editor_property('effect_type',fx)
    d.set_editor_property('content_category',unreal.GamePlatformVFXContentCategory.ENVIRONMENT)
    for key,value in (('auto_destroy',False),('allow_pooling',True),('enable_scalability',True),('require_fixed_bounds',True),('max_lifetime_seconds',0.0)):
        d.set_editor_property(key,value)
    schema=unreal.GamePlatformVFXParameterSchema()
    # 所有参数都有真实默认值；调用方只覆盖希望调整的参数。0数由场景控制器取消请求。
    # 最小中心距含现有最大模型缩放与振翅余量；现有资产须走--repair-spacing预检真实网格后增量更新。
    rules=[('BirdCount','Integer',0,512),('FlockRadius','Float',200,10000),('FlockHeight','Float',200,10000),('FlightSpeed','Float',10,5000),('FlockSeed','Integer',0,1000000),('FlightMode','Integer',0,2),('WindInfluence','Float',0,2000),('FlockMinimumSpacing','Float',1,1000)]
    text='(MaxOverrideCount=8,Rules=('+','.join('(Name="User.%s",Type=%s,bRequired=False,MinValue=%s,MaxValue=%s)'%r for r in rules)+'))'
    assert schema.import_text(text)
    d.set_editor_property('parameter_schema',schema)
    params=unreal.GamePlatformVFXParameters()
    params.set_editor_property('integer_parameters',{'User.BirdCount':count,'User.FlockSeed':137,'User.FlightMode':mode})
    params.set_editor_property('float_parameters',{'User.FlockRadius':float(radius),'User.FlockHeight':float(height),'User.FlightSpeed':float(speed),'User.WindInfluence':80.0,'User.FlockMinimumSpacing':{'Seagull':220.0,'Swallow':140.0,'Eagle':340.0}[species]})
    d.set_editor_property('default_parameters',params)
    assert a.save_loaded_asset(d,only_if_is_dirty=False)
    print('BIRD_DEFINITION_SAVED',d.get_path_name())
print('BIRD_DEFINITIONS_COMPLETE',len(SPECIES))
