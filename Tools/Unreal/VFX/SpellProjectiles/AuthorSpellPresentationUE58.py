"""当前法术表现唯一Definition绑定作者（UE5.8，经Monolith调用）。

系统迁移先由Monolith完成；本入口不生成Niagara、材质或视觉版本，只绑定唯一合同、
补齐施法输入schema并写显式Cook标签。默认audit；显式apply()才保存，历史作者均失败关闭。
"""
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import re
import unreal as u

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from SpellPresentationContract import ROOT, load_manifest, required_packages, inspect_migration_source_candidates

OUT = ROOT / 'Saved/Validation/SingleImplementationConvergence20261027/SpellPresentation'


def migrate():
    """仅供任务根代理经Monolith显式调用：按合同原位rename，不复制系统，不删除redirector。"""
    assert Path(u.Paths.get_project_file_path()).resolve() == ROOT / 'Game/DivineBeastsArena.uproject'
    assert u.SystemLibrary.get_engine_version().startswith('5.8.')
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '存在未保存地图，停止'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '存在未保存内容，停止'
    data = load_manifest()
    pack = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'
    planned, mapping = [], []
    registry = u.AssetRegistryHelpers.get_asset_registry()
    options = u.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    queue = [r['system'] if u.load_asset(r['system']) else r['source_system'] for r in data['bindings']]
    visited, dependencies = set(), []
    registry_candidates = set(queue)
    while queue:
        package = queue.pop()
        if package in visited:
            continue
        visited.add(package)
        for value in registry.get_dependencies(package, options):
            path = str(value)
            registry_candidates.add(path)
            if path.startswith('/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/'):
                dependencies.append(path)
                queue.append(path)
    directories = {'NiagaraScript': 'Niagara', 'Material': 'Materials', 'MaterialInstanceConstant': 'Materials',
                   'Texture2D': 'Textures', 'StaticMesh': 'Meshes'}
    explicit_systems = {r['source_system'] for r in data['bindings']} | {r['system'] for r in data['bindings']}
    occupied = {r['system'] for r in data['bindings']}
    for path in sorted(set(dependencies) - explicit_systems):
        asset = u.load_asset(path)
        assert asset is not None, '原生依赖不可读取: ' + path
        cls = asset.get_class().get_name()
        assert cls in directories, '未经批准的Review依赖类型，拒绝静默遗漏: ' + path + ': ' + cls
        leaf = re.sub(r'_(?:ConceptReview|Review|V3)(?=_|$)', '', path.rsplit('/', 1)[1])
        destination = '/DBAPresentationPack_Core/VFX/SpellProjectiles/' + directories[cls] + '/' + leaf
        assert destination not in occupied and not u.EditorAssetLibrary.does_asset_exist(destination), '依赖目标重名: ' + destination
        occupied.add(destination)
        directory, name = destination.rsplit('/', 1)
        planned.append(u.AssetRenameData(asset=asset, new_package_path=directory, new_name=name))
        mapping.append({'source': path, 'target': destination, 'class': cls})
    for row in data['bindings']:
        current = u.load_asset(row['system'])
        if current:
            assert current.get_path_name().split('.')[0] == row['system'], '规范路径指向非自身对象'
            continue
        source = u.load_asset(row['source_system'])
        assert isinstance(source, u.NiagaraSystem), row['source_system']
        directory, name = row['system'].rsplit('/', 1)
        planned.append(u.AssetRenameData(asset=source, new_package_path=directory, new_name=name))
        mapping.append({'source': row['source_system'], 'target': row['system'], 'class': 'NiagaraSystem'})
    # 此Package查询与已读引擎AssetRenameManager的RegistryPackage/NoRequirements匹配；
    # 它预测普通源/引用保存集，不等于截获rename内部全部保存。先补本地化/外部源候选预检。
    backup_packages = {v['source'] for v in mapping} | {r['definition'] for r in data['bindings']}
    referencers = {v['source']: [str(p) for p in registry.get_referencers(v['source'], options)] for v in mapping}
    backup_packages.update(p for values in referencers.values() for p in values)
    mounts = {p.stem: p.parent / 'Content' for p in (ROOT / 'Game/Plugins').rglob('*.uplugin')}
    mounts['Game'] = ROOT / 'Game/Content'
    candidate_preflight = inspect_migration_source_candidates([v['source'] for v in mapping], mounts, registry_candidates | backup_packages)
    for path in sorted(backup_packages):
        mount, relative = path.lstrip('/').split('/', 1)
        assert mount in mounts, '反向引用挂载点未知: ' + path
        source_file = mounts[mount] / (relative + '.uasset')
        if not source_file.exists():
            source_file = mounts[mount] / (relative + '.umap')
        assert source_file.exists(), '备份包不存在: ' + path
        backup = OUT / 'Backup' / source_file.relative_to(ROOT)
        backup.parent.mkdir(parents=True, exist_ok=True)
        if not backup.exists():
            shutil.copy2(source_file, backup)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'NativeMigrationPlan.json').write_text(json.dumps({'Mapping': mapping, 'Referencers': referencers, 'SourceCandidatePreflight': candidate_preflight,
                'SaveBoundary': 'rename_assets saves internally before returning; predicted backup set plus post-call residual dirty diagnostics'}, ensure_ascii=False, indent=2), 'utf-8')
    if planned:
        assert u.AssetToolsHelpers.get_asset_tools().rename_assets(planned), '原生rename失败，停止后续发布'
    # 保存实际规范对象；保留旧路径引擎redirector以供既有引用追踪，禁止生成第二套真实系统。
    for row in data['bindings']:
        system = u.load_asset(row['system'])
        assert isinstance(system, u.NiagaraSystem)
        assert u.EditorAssetLibrary.save_loaded_asset(system, False), row['system']
    for row in mapping:
        assert u.EditorAssetLibrary.save_asset(row['target'], False), row['target']
    # rename_assets返回前已经可能保存源/目标/引用。这里仅检查调用后残留dirty并窄保存；
    # 不是阻止或回滚引擎内部保存的前置门禁，不能以此宣称没有其它包曾被引擎保存。
    permitted = backup_packages | {v['target'] for v in mapping}
    dirty = list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages()) + list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    unexpected = [p.get_name() for p in dirty if p.get_name() not in permitted]
    assert not unexpected, '出现计划外dirty包，拒绝保存: ' + ','.join(unexpected)
    if dirty:
        assert u.EditorLoadingAndSavingUtils.save_packages(dirty, True), '计划内迁移引用包保存失败'
    return {'Renamed': len(planned), 'Mapping': mapping, 'NativeAuditRequired': True}


def audit(name='NativeAudit.json'):
    """调用独立只读原生门禁；返回真实读回结果，失败不改绑定。"""
    path = ROOT / 'Tests/Assets/ValidateSpellPresentationUE58.py'
    spec = importlib.util.spec_from_file_location('spell_presentation_native_validation', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.validate(OUT / name, raise_on_error=False)


def apply():
    """所有前置验证通过后才串行绑定；保留原有schema其它规则及稳定ID。"""
    assert Path(u.Paths.get_project_file_path()).resolve() == ROOT / 'Game/DivineBeastsArena.uproject'
    assert u.SystemLibrary.get_engine_version().startswith('5.8.')
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '存在未保存地图，停止'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '存在未保存内容，停止'
    data = load_manifest()
    assets = []
    for row in data['bindings']:
        definition = u.load_asset(row['definition'])
        system = u.load_asset(row['system'])
        assert definition and isinstance(system, u.NiagaraSystem), row['system']
        logical = definition.get_editor_property('logical_id')
        identity = str(logical.namespace) + '.' + str(logical.name) + '@' + str(logical.logical_version)
        assert identity == row['logical_id'], '稳定ID异常，拒绝修改: ' + row['definition']
        actual = definition.get_editor_property('niagara_system')
        actual = actual.get_path_name().split('.')[0] if actual else ''
        assert actual in (row['source_system'], row['system']), '未授权的当前绑定: ' + actual
        assert not definition.get_editor_property('quality_variants') and not definition.get_editor_property('platform_variants'), '拒绝忽略覆盖路径'
        assets.append((row, definition, system))
    # 全部Cook种子必须是真实可加载资源；缺一项不得发布半套标签。
    explicit = []
    for path in required_packages(data):
        if path != data['cook_label']:
            asset = u.load_asset(path)
            assert asset is not None, 'Cook种子缺失: ' + path
            explicit.append(asset)
    audit('NativeBefore.json')
    pack = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'
    for path in [v['definition'] for v in data['bindings']] + [data['cook_label']]:
        source = pack / (path.removeprefix('/DBAPresentationPack_Core/') + '.uasset')
        if source.exists():
            backup = OUT / 'Backup' / source.relative_to(ROOT)
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                shutil.copy2(source, backup)
    for row, definition, system in assets:
        definition.set_editor_property('niagara_system', system)
        if row['require_cast_parameters']:
            schema = definition.get_editor_property('parameter_schema')
            retained = [r for r in schema.rules if str(r.name) not in data['cast_parameters']]
            retained.extend(u.GamePlatformVFXParameterRule(name=key, type=u.GamePlatformVFXParameterType.FLOAT,
                            min_value=limits[0], max_value=limits[1], required=True)
                            for key, limits in data['cast_parameters'].items())
            schema.set_editor_property('rules', retained)
            definition.set_editor_property('parameter_schema', schema)
        assert u.EditorAssetLibrary.save_loaded_asset(definition, False), row['definition']
    label = u.load_asset(data['cook_label'])
    if label is None:
        factory = u.DataAssetFactory()
        factory.set_editor_property('data_asset_class', u.PrimaryAssetLabel)
        directory, name = data['cook_label'].rsplit('/', 1)
        label = u.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, u.PrimaryAssetLabel, factory)
    assert isinstance(label, u.PrimaryAssetLabel)
    label.set_editor_property('is_runtime_label', False)
    label.set_editor_property('label_assets_in_my_directory', False)
    label.set_editor_property('explicit_assets', explicit)
    label.set_editor_property('rules', u.PrimaryAssetRules(priority=1, apply_recursively=True,
                              cook_rule=u.PrimaryAssetCookRule.ALWAYS_COOK))
    assert u.EditorAssetLibrary.save_loaded_asset(label, False)
    result = audit('NativeAfter.json')
    assert result['Passed'], result['Errors']
    return result


if __name__ == '__main__':
    print(json.dumps(audit(), ensure_ascii=False))
