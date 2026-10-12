#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7–P9真实UE审核资源作者：音效试听图、十二生肖共享展示、逐插件人工用例。

只操作新的Foundation开发子树，发布配置已有NeverCook；不改正式前端/UI或既有游戏地图。
每个编辑器进程只创建一张地图，禁止跨图保留UWorld指针引起GC崩溃。
声音来自实际项目音效内容包；英雄按既有Appearance载入，共用Mannequin原型不冒充最终生肖。
用例复用GamePlatformDeveloperTools.ReviewCase；不创建Passed报告、不代签人工。
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
BASE='/Game/Development/Foundation/PluginReviews'
SFX_DEFS='/Game/Development/Foundation/Definitions/P7SFX'
OUT=ROOT/'Saved/Validation/PluginP7P9_20261012'
HEROES=('Rat','Ox','Tiger','Rabbit','Dragon','Snake','Horse','Goat','Monkey','Rooster','Dog','Boar')
MODE=os.environ.get('P7P9_AUTHOR_MODE','inspect').lower()
TASK=os.environ.get('P7P9_AUTHOR_TASK','').lower()
ASSETS=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()


def guard():
    project=str(unreal.Paths.get_project_file_path()).replace('\\','/')
    if not project.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'):
        raise RuntimeError('拒绝在非正式神兽联盟工程中创建审核资源')
    if not str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'):
        raise RuntimeError('必须是锁定UE5.8')
    config=(ROOT/'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
    if 'DirectoriesToNeverCook=(Path="/Game/Development/Foundation")' not in config:
        raise RuntimeError('缺少开发资源发布排除')
    if MODE not in ('inspect','apply') or TASK not in ('sfx','heroes','cases'):
        raise ValueError('使用P7P9_AUTHOR_MODE=inspect/apply和P7P9_AUTHOR_TASK=sfx/heroes/cases')
    OUT.mkdir(parents=True,exist_ok=True)


def directory(path):
    if not ASSETS.does_directory_exist(path) and not ASSETS.make_directory(path):
        raise RuntimeError('无法建立审核目录：'+path)


def load(path,cls):
    obj=ASSETS.load_asset(path)
    if not isinstance(obj,cls):raise TypeError('资产类型不符或缺失：'+path+'；要求'+cls.__name__)
    return obj


def soft_load(value,cls):
    if isinstance(value,cls):return value
    if hasattr(value,'to_soft_object_path'):value=value.to_soft_object_path()
    text=value.to_string() if hasattr(value,'to_string') else str(value)
    if not text or text=='None':raise RuntimeError('必要软引用为空')
    return load(text,cls)


def save(obj):
    if not ASSETS.save_loaded_asset(obj,only_if_is_dirty=False):
        raise RuntimeError('真实UE资产未保存：'+obj.get_path_name())


def create_data(folder,name,cls):
    path=folder+'/'+name
    if ASSETS.does_asset_exist(path):return load(path,cls),False
    factory=unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class',cls)
    obj=TOOLS.create_asset(name,folder,cls,factory)
    if not isinstance(obj,cls):raise RuntimeError('DataAsset创建失败：'+path)
    return obj,True


def spawn(cls,label,loc,rot=(0.0,0.0,0.0)):
    actor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        cls,unreal.Vector(*loc),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
    if actor is None:raise RuntimeError('无法创建审核演员：'+label)
    actor.set_actor_label(label)
    return actor


def mesh_box(label,location,scale):
    actor=spawn(unreal.StaticMeshActor,label,location)
    component=actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_static_mesh(load('/Engine/BasicShapes/Cube',unreal.StaticMesh))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def base_world(map_name):
    path=BASE+'/Maps/'+map_name
    if ASSETS.does_asset_exist(path):
        raise FileExistsError('审核地图已存在，拒绝重建覆盖：'+path)
    directory(BASE+'/Maps')
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level.new_level(path):raise RuntimeError('没有创建真实World：'+path)
    mesh_box(map_name+'_Floor',(0,0,-60),(52,40,1))
    sun=spawn(unreal.DirectionalLight,map_name+'_Sun',(-1200,-900,2800),(-45,35,0))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',4.0)
    spawn(unreal.SkyAtmosphere,map_name+'_Atmosphere',(0,0,0))
    spawn(unreal.SkyLight,map_name+'_SkyLight',(0,0,1200))
    spawn(unreal.PlayerStart,map_name+'_PlayerStart',(0,-2300,160),(0,90,0))
    camera=spawn(unreal.CameraActor,map_name+'_Camera',(0,-3000,1000),(-15,90,0))
    camera.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
    return level,path


def label_world(text,location):
    actor=spawn(unreal.TextRenderActor,'Label_'+text.replace(' ','_'),location,(0,-90,0))
    component=actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(unreal.Text(text))
    component.set_world_size(42.0)
    return actor


def author_sfx():
    actor_cls=getattr(unreal,'GamePlatformSFXReviewActor',None)
    if actor_cls is None:raise RuntimeError('新的SFX审核Actor未加载，不能替代为直接Audio播放')
    directory(BASE+'/Audio');directory(SFX_DEFS)
    sound=load('/DBASFXPack_Core/Weather/Audio/SW_DBA_Weather_RainLight_Loop',unreal.SoundWave)
    wind=load('/DBASFXPack_Core/Weather/Audio/SW_DBA_Weather_Wind_Loop',unreal.SoundWave)
    attenuation_path=BASE+'/Audio/SA_GP_ReviewDistance'
    if ASSETS.does_asset_exist(attenuation_path):
        attenuation=load(attenuation_path,unreal.SoundAttenuation)
    else:
        attenuation=TOOLS.create_asset('SA_GP_ReviewDistance',BASE+'/Audio',unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
        if not isinstance(attenuation,unreal.SoundAttenuation):raise RuntimeError('衰减资产创建失败')
        settings=attenuation.get_editor_property('attenuation')
        settings.set_editor_property('attenuation_shape_extents',unreal.Vector(180,0,0))
        settings.set_editor_property('falloff_distance',1800.0)
        attenuation.set_editor_property('attenuation',settings)
        save(attenuation)
    concurrency_path=BASE+'/Audio/SCN_GP_ReviewBounded'
    if ASSETS.does_asset_exist(concurrency_path):
        concurrency=load(concurrency_path,unreal.SoundConcurrency)
    else:
        concurrency=TOOLS.create_asset('SCN_GP_ReviewBounded',BASE+'/Audio',unreal.SoundConcurrency,unreal.SoundConcurrencyFactory())
        if not isinstance(concurrency,unreal.SoundConcurrency):raise RuntimeError('并发资产创建失败')
        settings=concurrency.get_editor_property('concurrency')
        settings.set_editor_property('max_count',4)
        concurrency.set_editor_property('concurrency',settings)
        save(concurrency)
    stations=[]
    for name,kind,source in (('TwoD',unreal.GamePlatformSFXPlaybackSpace.TWO_D,sound),
                            ('World',unreal.GamePlatformSFXPlaybackSpace.WORLD,wind),
                            ('Attached',unreal.GamePlatformSFXPlaybackSpace.ATTACHED,sound)):
        obj,new=create_data(SFX_DEFS,'DA_GP_ReviewSFX_'+name,unreal.GamePlatformSFXDefinition)
        logical='review.sfx.'+name.lower()+'@1'
        if new:
            identity=unreal.GamePlatformId()
            identity.set_editor_property('namespace','review.sfx')
            identity.set_editor_property('name',name.lower())
            identity.set_editor_property('logical_version',1)
            obj.set_editor_property('logical_id',identity)
            obj.set_editor_property('data_version',unreal.GamePlatformDataVersion())
            obj.set_editor_property('sound',source)
            obj.set_editor_property('playback_space',kind)
            obj.set_editor_property('concurrency',concurrency)
            if name!='TwoD':obj.set_editor_property('attenuation',attenuation)
            obj.set_editor_property('volume_multiplier',0.35)
            obj.set_editor_property('fade_in_seconds',0.1)
            obj.set_editor_property('fade_out_seconds',0.2)
            save(obj)
        stations.append((name,logical))
    level,path=base_world('L_GP_SFXReview')
    for index,(name,logical) in enumerate(stations):
        x=(index-1)*1000
        mesh_box('SFX_'+name+'_Marker',(x,0,55),(2.0,2.0,1.2))
        actor=spawn(actor_cls,'SFXReview_'+name,(x,0,180))
        actor.set_editor_property('definition_id',unreal.Name(logical))
        actor.set_editor_property('play_on_begin_play',False)
        actor.set_editor_property('start_delay_seconds',0.3+index*1.2)
        actor.set_editor_property('volume_multiplier',0.35)
        label_world(name+' SFX - Manual PlayReview',(x-190,-350,200))
    if not level.save_current_level():raise RuntimeError('SFX审核图保存失败')
    return {'map':path,'stations':[x[0] for x in stations],'definitions':[x[1] for x in stations],
            'autoplay':False,'manualAcceptance':'pending','audioOrigin':'workspace_synthetic_prototype'}


def author_heroes():
    cls=getattr(unreal,'GamePlatformCharacterPreviewStage',None)
    appearance_cls=getattr(unreal,'DivineBeastsCharacterAppearanceProfile',None)
    if cls is None or appearance_cls is None:raise RuntimeError('角色展示/Appearance原生反射模块缺失')
    # 首先预检12套必要引用，失败时不创建半张声称完整的地图。
    ready=[]
    for hero in HEROES:
        path='/DBAHeroPack_'+hero+'/Characters/DA_Appearance_Zodiac_'+hero
        profile=load(path,appearance_cls)
        expected='Hero.Zodiac.'+hero
        if str(profile.get_editor_property('hero_definition_id'))!=expected:
            raise RuntimeError('英雄身份与其内容包不符：'+hero)
        mesh=soft_load(profile.get_editor_property('skeletal_mesh'),unreal.SkeletalMesh)
        materials=[soft_load(x,unreal.MaterialInterface) for x in profile.get_editor_property('material_overrides')]
        if not materials:raise RuntimeError('英雄材质列表为空：'+hero)
        ready.append((hero,profile,mesh,materials))
    level,path=base_world('L_DBA_HeroShowcase')
    records=[]
    for index,(hero,profile,mesh,materials) in enumerate(ready):
        x=(index%4-1.5)*900;y=(index//4-1)*850
        mesh_box('HeroPlatform_'+hero,(x,y,10),(4.0,3.5,0.4))
        stage=spawn(cls,'HeroShowcase_'+hero,(x,y,110))
        applied=stage.apply_preview_appearance(mesh,materials,None,
            profile.get_editor_property('mesh_relative_location'),
            profile.get_editor_property('mesh_relative_rotation'),
            profile.get_editor_property('mesh_relative_scale'))
        if applied is not True:raise RuntimeError('正式平台预览舞台拒绝外观：'+hero)
        placeholder=bool(profile.get_editor_property('development_placeholder'))
        label_world(hero+(' - PROTOTYPE' if placeholder else ' - REVIEW'),(x-230,y-300,65))
        records.append({'hero':hero,'profile':profile.get_path_name(),'mesh':mesh.get_path_name(),
                        'materials':[m.get_path_name() for m in materials],
                        'prototype':placeholder,'animationClassApplied':False})
    if not level.save_current_level():raise RuntimeError('共享英雄展示图保存失败')
    return {'map':path,'heroes':records,'count':len(records),'manualAcceptance':'pending',
            'scope':'外观原型展示，不代表技能/动画/联机完整交付'}


def author_cases():
    cls=getattr(unreal,'GamePlatformReviewCase',None)
    if cls is None:raise RuntimeError('现有ReviewCase反射类未加载，不能新造另一审核系统')
    folder=BASE+'/Cases';directory(folder)
    rows=[]
    for desc in sorted((ROOT/'Game/Plugins').rglob('*.uplugin')):
        if any(x in ('Intermediate','Binaries','Saved') for x in desc.parts):continue
        data=json.loads(desc.read_text(encoding='utf-8-sig'))
        plugin=desc.stem
        content='/ContentPacks/' in desc.as_posix()
        obj,new=create_data(folder,'RC_'+plugin,cls)
        if new:
            obj.set_editor_property('case_id',unreal.Name('Review.'+plugin))
            obj.set_editor_property('category',unreal.Name('Content' if content else 'Plugin'))
            description=data.get('Description',plugin)
            obj.set_editor_property('description',plugin+'：'+description+'；逐项记录适用门禁，不按文件存在签收。')
            obj.set_editor_property('preconditions',['锁定UE5.8、当前源码与构建输入匹配','插件依赖加载成功；目标平台与资源版本明确','生产凭据/收费不可用于演示'])
            obj.set_editor_property('steps',['核对当前插件的资源、接口与已声明测试','运行适用自动化/共享审核场景并保存日志','核对失败、取消、世界退出/资源释放（适用时）','检查Cook/服务端隔离和设备性能（适用时）','由真实审核人记录通过或驳回，自动化不可代签'])
            obj.set_editor_property('expected_result','所有适用G0–G5均有当前可追溯证据；缺失项保持待审/阻塞，原型不冒充最终资源。')
            obj.set_editor_property('required_evidence',['源码/资源SHA及构建目标','官方测试或真实资源重载结果','适用的运行截图/音频/服务器及性能证据','真实审核者、日期和人工结论'])
            save(obj)
        rows.append({'plugin':plugin,'descriptor':desc.relative_to(ROOT).as_posix(),
                     'reviewCase':obj.get_path_name(),'manualStatus':'NotRun'})
    return {'cases':rows,'count':len(rows),'humanConfirmed':False}


def main():
    guard()
    if MODE=='inspect':
        print('P7P9_ASSET_PLAN',TASK,BASE)
        return
    result={'sfx':author_sfx,'heroes':author_heroes,'cases':author_cases}[TASK]()
    result['task']=TASK;result['engine']=str(unreal.SystemLibrary.get_engine_version())
    (OUT/('Author_'+TASK+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('P7P9_AUTHOR_OK',TASK,result.get('count',len(result.get('stations',[]))))
if __name__=='__main__':main()
