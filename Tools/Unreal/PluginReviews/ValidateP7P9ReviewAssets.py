#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第三批真实UE资产回读：全内容包、65用例及单进程单地图检查；不修改/保存资源。

P7P9_VALIDATE_TASK=assets核对所有当前登记内容包的真实uasset并生成哈希；
=sfx/heroes只读取单一新审核地图，避免World指针跨图保留。
源资产类型、编辑器地图、运行效果、人工审核严格分开，不生成伪Passed视觉结论。
"""
from __future__ import annotations
import collections
import hashlib
import json
import os
import traceback
from pathlib import Path
import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginP7P9_20261012'
BASE='/Game/Development/Foundation/PluginReviews'
TASK=os.environ.get('P7P9_VALIDATE_TASK','assets').lower()
ASSETS=unreal.EditorAssetLibrary


def required(path,klass):
    obj=ASSETS.load_asset(path)
    if not isinstance(obj,klass):raise RuntimeError('未加载期望类别的真实资产：'+path+' / '+klass.__name__)
    return obj


def check_cases():
    descriptors=[p for p in (ROOT/'Game/Plugins').rglob('*.uplugin')
                 if not any(s in ('Intermediate','Binaries','Saved') for s in p.parts)]
    cls=getattr(unreal,'GamePlatformReviewCase',None)
    if cls is None:raise RuntimeError('现有审核用例类型未加载')
    ids=set();rows=[]
    for desc in sorted(descriptors):
        obj=required(BASE+'/Cases/RC_'+desc.stem,cls)
        case_id=str(obj.get_editor_property('case_id'))
        if case_id in ids:raise RuntimeError('审核身份重复：'+case_id)
        ids.add(case_id)
        for name in ('category','description','steps','expected_result','required_evidence'):
            if not obj.get_editor_property(name):raise RuntimeError('用例字段不完整：'+desc.stem+'/'+name)
        rows.append({'plugin':desc.stem,'case':obj.get_path_name(),'caseId':case_id,'humanStatus':'NotRun'})
    return rows


def check_content():
    registry=json.loads((ROOT/'Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json').read_text(encoding='utf-8-sig'))
    rows=[];failed=[];inputs={};total=0
    for entry in registry['ContentPacks']:
        folder=ROOT/'Game/Plugins/DivineBeasts/ContentPacks'/entry['RelativePath']
        native=folder/(entry['Name']+'.uplugin')
        if not native.is_file():raise RuntimeError('登记插件描述缺失：'+entry['Name'])
        counter=collections.Counter();errors=[];assets=sorted((folder/'Content').rglob('*.uasset'))
        for file in assets:
            virtual='/'+entry['Name']+'/'+file.relative_to(folder/'Content').with_suffix('').as_posix()
            obj=ASSETS.load_asset(virtual)
            digest=hashlib.sha256(file.read_bytes()).hexdigest()
            inputs[file.relative_to(ROOT).as_posix()]=digest
            if obj is None:
                errors.append(virtual);continue
            counter[obj.get_class().get_name()]+=1;total+=1
        if not assets:errors.append('empty_content_pack')
        if errors:failed.extend(errors)
        rows.append({'plugin':entry['Name'],'uassets':len(assets),'loaded':sum(counter.values()),
                     'classes':dict(counter),'errors':errors})
        print('P7P9_CONTENT_CHECK',entry['Name'],len(assets),'errors',len(errors))
    # 文件检查前后必须对应同一输入，不把其他会话改写中的资源作为稳定通过。
    changed=[path for path,sha in inputs.items() if hashlib.sha256((ROOT/path).read_bytes()).hexdigest()!=sha]
    if changed:failed.extend(changed)
    cases=check_cases()
    return {'status':'passed' if not failed else 'failed','contentPacks':rows,'packCount':len(rows),
            'loadedAssets':total,'cases':cases,'caseCount':len(cases),'errors':failed,
            'inputs':inputs,'manualAcceptance':'pending','validationScope':'真实资产加载/类型/输入稳定性，不是运行或美术签审'}


def check_map():
    name={'sfx':'L_GP_SFXReview','heroes':'L_DBA_HeroShowcase'}[TASK]
    path=BASE+'/Maps/'+name
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not ASSETS.does_asset_exist(path) or not level.load_level(path):raise RuntimeError('无法独立重开审核图：'+path)
    subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors=list(subsystem.get_all_level_actors())
    results=[]
    if TASK=='sfx':
        cls=getattr(unreal,'GamePlatformSFXReviewActor',None)
        if cls is None:raise RuntimeError('SFX审核Actor未装配')
        stations=[x for x in actors if isinstance(x,cls)]
        if len(stations)!=3:raise RuntimeError('不是三种真实音效审核站')
        for actor in stations:
            label=actor.get_actor_label().removeprefix('SFXReview_')
            obj=required('/Game/Development/Foundation/Definitions/P7SFX/DA_GP_ReviewSFX_'+label,unreal.GamePlatformSFXDefinition)
            if str(actor.get_editor_property('definition_id'))!='review.sfx.'+label.lower()+'@1':
                raise RuntimeError('音效审核站未保存正确逻辑定义：'+label)
            if actor.get_editor_property('play_on_begin_play'):raise RuntimeError('审核站不应默认自动发声')
            source=obj.get_editor_property('sound')
            if not isinstance(source,unreal.SoundWave):
                source=ASSETS.load_asset(str(source))
            if not isinstance(source,unreal.SoundWave):raise RuntimeError('音效定义不是实际SoundWave：'+label)
            results.append({'station':label,'definition':obj.get_path_name(),'sound':source.get_path_name(),
                            'space':str(obj.get_editor_property('playback_space'))})
    else:
        cls=getattr(unreal,'GamePlatformCharacterPreviewStage',None)
        stations=[x for x in actors if isinstance(x,cls)] if cls else []
        if len(stations)!=12:raise RuntimeError('共享英雄审核图不是12个真实平台预览舞台')
        expected=json.loads((OUT/'Author_heroes.json').read_text(encoding='utf-8'))['heroes']
        source={item['hero']:item for item in expected}
        for actor in stations:
            hero=actor.get_actor_label().removeprefix('HeroShowcase_')
            if hero not in source:raise RuntimeError('未知英雄展示对象：'+hero)
            component=actor.get_preview_mesh_component()
            mesh=component.get_skeletal_mesh_asset()
            if not isinstance(mesh,unreal.SkeletalMesh) or mesh.get_path_name()!=source[hero]['mesh']:
                raise RuntimeError('重新加载后英雄网格不匹配：'+hero)
            material=component.get_material(0)
            if material is None or material.get_path_name()!=source[hero]['materials'][0]:
                raise RuntimeError('重新加载后英雄材质不匹配：'+hero)
            results.append({'hero':hero,'mesh':mesh.get_path_name(),'material':material.get_path_name(),
                            'prototype':source[hero]['prototype']})
    return {'status':'passed','map':path,'entries':results,'count':len(results),
            'manualAcceptance':'pending','scope':'独立编辑器地图对象及真实资产引用回读'}


def main():
    if TASK not in ('assets','sfx','heroes'):raise RuntimeError('未知审核任务')
    if not str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'):
        raise RuntimeError('不是UE5.8')
    if not str(unreal.Paths.get_project_file_path()).replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'):
        raise RuntimeError('不是神兽联盟工作区主工程')
    OUT.mkdir(parents=True,exist_ok=True)
    try:result=check_content() if TASK=='assets' else check_map()
    except Exception as exc:
        result={'status':'failed','error':str(exc),'traceback':traceback.format_exc(),'task':TASK}
        (OUT/('Validate_'+TASK+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        raise
    result['task']=TASK
    (OUT/('Validate_'+TASK+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    if result['status']!='passed':raise RuntimeError('内容包有资产无法加载，详见真实报告')
    print('P7P9_ASSET_VALIDATION_OK',TASK,result.get('count',result.get('loadedAssets',0)))
if __name__=='__main__':main()
