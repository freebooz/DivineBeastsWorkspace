#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群发布收尾只读审核：复用实际P4门禁，增加依赖闭包、旧模型引用与两张场景配置检查。

仅在独立UE5.8编辑器进程执行，临时场景不保存；不删除旧网格、不重建Niagara、不修改主编辑器。
报告保存到Saved/Validation/BirdFlockDelivery，以源资源前后哈希拒绝并发变化造成的假通过。
本报告不等价于当前C++运行、GPU性能或人工审美验收。
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT/'Saved/Validation/BirdFlockDelivery'
DBA = '/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
GP = '/GamePlatformVFX/Environment/BirdFlock'
CONTENT = ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock'
HISTORY = CONTENT.parents[2]/'Development/History/BirdFlock/Meshes'
HISTORY_ROOT = '/DBAPresentationPack_Core/Development/History/BirdFlock/Meshes'
EXPECTED = {'Seagull': 32, 'Swallow': 72, 'Eagle': 5}
MAP_ROOT = '/Game/Development/BirdFlock/Maps/'


def snapshot():
    """仅记录本次有关资产及代码，前后精确比较；不读取账号配置、用户存档或凭据。"""
    files = list(CONTENT.rglob('*.uasset'))
    files += list(HISTORY.glob('*.uasset'))
    files += list((ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock').rglob('*.uasset'))
    files += list((ROOT/'Game/Content/Development/BirdFlock').rglob('*.umap'))
    source = ROOT/'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient'
    files += list(source.glob('Private/Environment/*BirdFlock*.cpp'))
    files += list(source.glob('Public/Environment/*BirdFlock*.h'))
    files += [source/'Private/DivineBeastsPresentationClientSubsystem.cpp']
    return {str(p.relative_to(ROOT)).replace('\\','/'): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(set(files)) if p.is_file()}


def dependency_closure(registry, options, seed):
    """遍历项目/平台包闭包；引擎官方内部图不展开，上限4096防止坏引用拖垮审核工具。"""
    pending = [seed]
    seen = set()
    all_dependencies = set()
    while pending:
        path = pending.pop()
        if path in seen:
            continue
        seen.add(path)
        if len(seen) > 4096:
            raise RuntimeError('依赖闭包超过审核上限：'+seed)
        dependencies = {str(v) for v in registry.get_dependencies(path, options)}
        all_dependencies.update(dependencies)
        pending.extend(v for v in dependencies
                       if v not in seen and v.startswith(('/DBA', '/GamePlatform', '/Moba', '/Game/')))
    return sorted(all_dependencies)


def inspect_map(path):
    """通过LevelEditor加载自身审核地图；函数返回仅含标量，避免持有旧World导致切图泄漏。"""
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(path):
        raise RuntimeError('审核关卡加载失败：'+path)
    rows = []
    component_type = unreal.DivineBeastsBirdFlockSceneComponent
    for actor in actors.get_all_level_actors():
        for component in actor.get_components_by_class(component_type):
            rows.append({'actor': actor.get_actor_label(), 'species': str(component.get_editor_property('species_id')),
                         'count': int(component.get_editor_property('bird_count')),
                         'flockId': str(component.get_editor_property('flock_id'))})
    if path.endswith('IntegrationReview'):
        if len(rows) != 3 or {r['species']: r['count'] for r in rows} != EXPECTED:
            raise RuntimeError('正式审核场景三种类型/数量不符：'+repr(rows))
        ids = [row['flockId'] for row in rows]
        if len(set(ids)) != 3 or any(value in ('', 'None') for value in ids):
            raise RuntimeError('审核场景鸟群标识重复或空缺')
    return {'map': path, 'sceneComponents': rows, 'savedByAudit': False}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = {'status': 'running', 'utc': datetime.now(timezone.utc).isoformat(),
              'errors': [], 'manualAcceptance': 'pending', 'runtimeVerified': False}
    before = snapshot()
    try:
        project = unreal.Paths.get_project_file_path().replace('\\','/')
        if not project.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'):
            raise RuntimeError('不是授权的唯一工程')
        if not unreal.SystemLibrary.get_engine_version().startswith('5.8.'):
            raise RuntimeError('不是锁定UE5.8')
        report['project'] = project
        # 复用同一真实资产检查，不复制第二套预设/类型规则，不覆盖上轮P4报告。
        spec = importlib.util.spec_from_file_location('bird_existing_p4_gate', Path(__file__).with_name('ValidateBirdFlockP4Assets.py'))
        if spec is None or spec.loader is None:
            raise RuntimeError('缺少P4资产校验入口')
        gate = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(gate)
        gate.OUT = OUT/'AssetReadback'
        gate.main()
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True)
        report['formalDependencies'] = []
        for species in EXPECTED:
            path = DBA+'/Niagara/NS_DBA_BirdFlock_'+species
            closure = dependency_closure(registry, options, path)
            expected_mesh = DBA+'/Meshes/Flight/SM_DBA_Bird_'+species
            if expected_mesh not in closure:
                raise RuntimeError('正式Niagara没有引用Flight模型：'+species)
            legacy = [p for p in closure if p.startswith(DBA+'/Meshes/SM_DBA_Bird_') or p.startswith(HISTORY_ROOT+'/')]
            if legacy:
                raise RuntimeError('正式Niagara仍依赖旧原型：'+repr(legacy))
            report['formalDependencies'].append({'species': species, 'system': path,
                'flightMesh': expected_mesh, 'legacyDependencies': legacy, 'closure': closure})
        report['legacyInventory'] = []
        for file in sorted(HISTORY.glob('SM_DBA_Bird_*.uasset')):
            package = HISTORY_ROOT+'/'+file.stem
            refs = sorted(str(v) for v in (registry.get_referencers(package, options) or []))
            report['legacyInventory'].append({'asset': package, 'referencers': refs, 'sourceDeleted': False})
        report['maps'] = [inspect_map(MAP_ROOT+'L_DBA_BirdFlockReview'),
                          inspect_map(MAP_ROOT+'L_DBA_BirdFlockIntegrationReview')]
        after = snapshot()
        if before != after:
            raise RuntimeError('检查期间源码或资产被并行修改，本次证据不能作为稳定快照')
        report['sourceSha256'] = before
        report['status'] = 'passed'
        print('BIRD_DELIVERY_AUDIT_PASSED', 'species', len(EXPECTED),
              'legacyRetained', len(report['legacyInventory']), 'maps', len(report['maps']))
    except Exception:
        report['status'] = 'failed'
        report['errors'].append(traceback.format_exc())
        report['sourceSha256'] = before
        print('BIRD_DELIVERY_AUDIT_FAILED', report['errors'][-1])
        raise
    finally:
        (OUT/'AssetSceneAudit.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
