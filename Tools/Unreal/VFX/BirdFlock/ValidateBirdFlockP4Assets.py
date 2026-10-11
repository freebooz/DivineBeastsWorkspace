#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群P4真实资产回归门禁：独立UE5.8编辑器进程只读加载并验证场景蓝图。

此脚本先于本轮蓝图资产制作执行，以实际缺失资产证明红灯；之后复用同一门禁。
只向Saved/Validation写报告，不创建替代资源、不修复失败对象、不触碰其他场景。
检查物种WorldDefinition、Niagara引用、默认数量、真实厘米网格和平台依赖方向。
本门禁不等于GPU着色器编译、真实渲染或人眼验收。
"""
from pathlib import Path
import hashlib
import json
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'Saved/Validation/BirdFlockP4P7'
GP = '/GamePlatformVFX/Environment/BirdFlock'
DBA = '/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
BP = DBA + '/Blueprints/BP_DBA_BirdFlockAmbient'
EXPECTED = {'Seagull':32, 'Swallow':72, 'Eagle':5}


def asset_path(value):
    """统一解析已加载UObject或SoftObjectPath的资源身份，不静默转换成默认资源。"""
    if value is None:
        return ''
    if hasattr(value, 'get_path_name'):
        return str(value.get_path_name()).split('.')[0]
    return str(value).split('.')[0]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = {'status':'running','assets':[],'errors':[], 'manualAcceptance':'pending'}
    try:
        project = unreal.Paths.get_project_file_path().replace('\\','/')
        if not project.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'):
            raise RuntimeError('不是授权的神兽联盟工作空间：' + project)
        if not unreal.SystemLibrary.get_engine_version().startswith('5.8.'):
            raise RuntimeError('不是锁定UE5.8')
        assets = unreal.EditorAssetLibrary
        master = assets.load_asset(GP + '/Materials/M_GP_BirdFlock_Master')
        effect = assets.load_asset(GP + '/EffectTypes/NET_GP_BirdFlock_Ambient')
        if not isinstance(master, unreal.Material) or not isinstance(effect, unreal.NiagaraEffectType):
            raise RuntimeError('平台母材质或Niagara EffectType未真实存在')
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        options = unreal.AssetRegistryDependencyOptions()
        options.set_editor_property('include_soft_package_references', True)
        options.set_editor_property('include_hard_package_references', True)
        for name in ('/Materials/M_GP_BirdFlock_Master','/Niagara/NM_GP_BirdFlockMotion',
                     '/Niagara/NS_GP_BirdFlock_Base','/EffectTypes/NET_GP_BirdFlock_Ambient'):
            path = GP + name
            obj = assets.load_asset(path)
            if obj is None:
                raise RuntimeError('平台真实资产缺失：' + path)
            dependencies = [str(v) for v in registry.get_dependencies(path, options)]
            forbidden = [v for v in dependencies if v.startswith('/DBA') or v.startswith('/Moba')]
            if forbidden:
                raise RuntimeError('平台鸟群反向引用项目/MOBA资产：' + repr(forbidden))
            report['assets'].append({'path':path, 'type':obj.get_class().get_name(),
                                     'dependencies':dependencies})
        for species, count in EXPECTED.items():
            system_path = DBA + '/Niagara/NS_DBA_BirdFlock_' + species
            system = assets.load_asset(system_path)
            definition_path = DBA + '/Definitions/DA_DBA_VFX_BirdFlock_' + species
            definition = assets.load_asset(definition_path)
            mesh = assets.load_asset(DBA + '/Meshes/Flight/SM_DBA_Bird_' + species)
            material = assets.load_asset(DBA + '/Materials/MI_DBA_Bird_' + species)
            if not isinstance(system, unreal.NiagaraSystem):
                raise RuntimeError('物种Niagara缺失：' + species)
            if not isinstance(definition, unreal.GamePlatformVFXWorldDefinition):
                raise RuntimeError('物种不是正式平台WorldDefinition：' + species)
            if not isinstance(mesh, unreal.StaticMesh) or not isinstance(material, unreal.MaterialInstanceConstant):
                raise RuntimeError('物种网格/材质缺失：' + species)
            if asset_path(definition.get_editor_property('niagara_system')) != system_path:
                raise RuntimeError('WorldDefinition没有引用自身物种Niagara：' + species)
            if asset_path(definition.get_editor_property('effect_type')) != asset_path(effect):
                raise RuntimeError('Definition的EffectType不一致：' + species)
            if asset_path(system.get_editor_property('effect_type')) != asset_path(effect):
                raise RuntimeError('Niagara的EffectType不一致：' + species)
            params = definition.get_editor_property('default_parameters')
            actual = {str(k):int(v) for k,v in params.get_editor_property('integer_parameters').items()}
            if actual.get('User.BirdCount') != count:
                raise RuntimeError('物种默认数量与计划不一致：' + species + repr(actual))
            extent = mesh.get_bounds().box_extent
            maximum = max(abs(extent.x), abs(extent.y), abs(extent.z))
            if not 30.0 <= maximum <= 300.0:
                raise RuntimeError('鸟模型厘米尺寸异常：' + species + ' ' + str(maximum))
            if asset_path(material.get_editor_property('parent')) != asset_path(master):
                raise RuntimeError('项目材质不继承通用母版：' + species)
            report['assets'].append({'species':species, 'system':system_path,
                'definition':definition_path,'defaultCount':count,'maxExtentCm':maximum})
        # 不接受旧ISM原型蓝图，也不接受仅文件存在却无法加载生成类的占位资产。
        blueprint = assets.load_asset(BP) if assets.does_asset_exist(BP) else None
        if not isinstance(blueprint, unreal.Blueprint):
            raise RuntimeError('P4缺少真实项目鸟群场景蓝图：' + BP)
        klass = assets.load_blueprint_class(BP)
        component_type = getattr(unreal, 'DivineBeastsBirdFlockSceneComponent', None)
        if klass is None or component_type is None:
            raise RuntimeError('正式鸟群场景蓝图/项目组件反射类未加载')
        actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actor = actor_api.spawn_actor_from_class(klass, unreal.Vector(0,0,0), transient=True)
        if actor is None:
            raise RuntimeError('鸟群蓝图不能实例化')
        try:
            components = list(actor.get_components_by_class(component_type))
            if len(components) != 1:
                raise RuntimeError('鸟群蓝图必须恰好包含一个正式场景组件')
            component = components[0]
            options_list = list(component.get_bird_species_options())
            if set(options_list) != set(EXPECTED):
                raise RuntimeError('鸟种选择项不完整：' + repr(options_list))
            report['blueprint'] = {'asset':BP,'class':klass.get_path_name(),
                'component':component.get_class().get_path_name(),
                'speciesOptions':options_list,'defaultCount':int(component.get_editor_property('bird_count'))}
        finally:
            actor_api.destroy_actor(actor)
        report['status'] = 'passed'
        print('BIRD_P4_ASSET_VALIDATION_PASSED',len(report['assets']))
    except Exception:
        report['status'] = 'failed'
        report['errors'].append(traceback.format_exc())
        print('BIRD_P4_ASSET_VALIDATION_FAILED',report['errors'][-1])
        raise
    finally:
        (OUT/'P4AssetsValidation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')


if __name__ == '__main__':
    main()
