#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5正式表现链路审核关卡：复制本任务自己的GPU审核夹具，替换为项目鸟群蓝图。

不修改Village/OpenWorld/角色创建的正式地图；这些场景已由各自生命周期接入。
新审核地图用于隔离验证：项目场景组件→Presentation目录→Data租约→Niagara执行器。
只使用UE资产API生成World，不复制二进制字节/不覆盖已有人工改动。
"""
from pathlib import Path
import json
import os
import runpy
import unreal

ROOT = Path(__file__).resolve().parents[4]
SOURCE = '/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview'
TARGET = '/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview'
BLUEPRINT = '/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockAmbient'
SPECIES = {'Seagull':32, 'Swallow':72, 'Eagle':5}


def main():
    if os.getenv('BIRD_ISOLATED_REVIEW_AUTHOR') != '1':
        raise RuntimeError('只允许独立鸟群关卡作者进程')
    project = unreal.Paths.get_project_file_path().replace('\\','/')
    if not project.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'):
        raise RuntimeError('非授权的正式神兽联盟工程')
    if not unreal.SystemLibrary.get_engine_version().startswith('5.8.'):
        raise RuntimeError('需要锁定UE5.8')
    # 先按本任务已存在的校准工具修正GPU审核相机，保证复制来源正确。
    runpy.run_path(str(ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockNiagaraReviewMap.py'),run_name='__main__')
    assets = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    klass = assets.load_blueprint_class(BLUEPRINT)
    comp_class = getattr(unreal, 'DivineBeastsBirdFlockSceneComponent', None)
    if klass is None or comp_class is None:
        raise RuntimeError('正式鸟群蓝图/场景组件未就绪')
    if assets.does_asset_exist(TARGET):
        if not isinstance(assets.load_asset(TARGET), unreal.World):
            raise RuntimeError('目标路径被非地图资产占用')
        if not levels.load_level(TARGET):
            raise RuntimeError('无法回读已有链路审核地图')
        print('BIRD_INTEGRATION_MAP_REUSED',TARGET)
    else:
        # 普通AssetTools复制UWorld会留下Standalone对象；再LoadLevel触发旧世界泄漏断言。
        # 必须使用LevelEditor的模板地图工作流，由引擎统一卸载旧世界、复制、保存与切换。
        if not levels.new_level_from_template(TARGET, SOURCE):
            raise RuntimeError('无法通过UE模板地图工作流创建独立链路审核地图')
        for species,count in SPECIES.items():
            label='BirdReview_'+species
            matches=[a for a in actors.get_all_level_actors() if a.get_actor_label()==label]
            if len(matches)!=1 or not isinstance(matches[0],unreal.NiagaraActor):
                raise RuntimeError('原GPU夹具鸟群标识不唯一/类型不符：'+label)
            old=matches[0]
            location=old.get_actor_location()
            rotation=old.get_actor_rotation()
            # 源World已经复制，不会销毁原GPU审核地图的Actor。
            if not actors.destroy_actor(old):
                raise RuntimeError('无法移除副本中的直接Niagara演示Actor')
            new=actors.spawn_actor_from_class(klass,location,rotation)
            if new is None:
                raise RuntimeError('无法实例化正式项目鸟群蓝图')
            new.set_actor_label('BirdIntegration_'+species)
            components=list(new.get_components_by_class(comp_class))
            if len(components)!=1:
                raise RuntimeError('正式鸟群蓝图场景组件数量错误')
    # 每次单字段PostEditChange会重建SCS组件；不得对失效的旧组件连续写三个属性。
    # 本工具独占审核夹具按唯一标签恢复，并一次原子写入全部物种/数量设置。
    for species,count in SPECIES.items():
        matches=[a for a in actors.get_all_level_actors()
                 if a.get_actor_label()=='BirdIntegration_'+species]
        if len(matches)!=1:
            raise RuntimeError('审核地图自有鸟群Actor不唯一：'+species)
        components=list(matches[0].get_components_by_class(comp_class))
        if len(components)!=1:
            raise RuntimeError('审核地图正式鸟群组件不唯一：'+species)
        components[0].set_editor_properties({
            'flock_id':unreal.Name('DBA.Review.Birds.'+species),
            'species_id':unreal.Name(species), 'bird_count':count})
    if not levels.save_current_level():
        raise RuntimeError('链路审核地图保存失败')
    found=[]
    for actor in actors.get_all_level_actors():
        for component in actor.get_components_by_class(comp_class):
            found.append({'actor':actor.get_actor_label(),
                'flockId':str(component.get_editor_property('flock_id')),
                'species':str(component.get_editor_property('species_id')),
                'count':int(component.get_editor_property('bird_count'))})
    actual={f['species']:f['count'] for f in found}
    if len(found)!=3 or actual!=SPECIES:
        raise RuntimeError('三种物种/数量未正确存入正式场景组件：'+repr(found))
    report={'map':TARGET,'sourceFixture':SOURCE,'blueprint':BLUEPRINT,
            'flocks':found,'runtimePath':'SceneComponent->Presentation->Data->VFX->Niagara',
            'manualAcceptance':'pending'}
    out=ROOT/'Saved/Validation/BirdFlockP4P7';out.mkdir(parents=True,exist_ok=True)
    (out/'IntegrationReviewMap.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_INTEGRATION_REVIEW_MAP_PASSED',TARGET,len(found))


if __name__=='__main__':
    main()
