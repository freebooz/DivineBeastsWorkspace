# -*- coding: utf-8 -*-
"""SSLM项目层VFX迁移作者，由正确工程的Monolith游戏线程显式调用。

inspect读取外部包/依赖和源文件哈希；migrate使用平台Editor原生复制桥批量重映射并保存；
definitions创建现有GamePlatformVFX定义和原目录身份对照；verify复核外部依赖已清除。
不引入源库播放器、游戏规则、演示UI或自定义Catalog类；不改源文件，不覆盖已存在目标。
通过VFX_MIGRATION_ACTION选择阶段；失败保留结果与部分新资产，不伪称事务回滚。
"""
import collections
import csv
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
EVIDENCE = ROOT / 'Saved/Validation/PetalBloomIntegration20261015'
DEST = '/DBAPresentationPack_Core/VFX/Library'
SOURCE = Path('D:/GameVFX')
VISUAL_CLASSES = {'NiagaraSystem', 'NiagaraEmitter', 'NiagaraScript', 'NiagaraEffectType',
                  'Material', 'MaterialInstanceConstant', 'MaterialFunction', 'Texture2D',
                  'TextureCube', 'TextureRenderTarget2D', 'StaticMesh', 'CurveFloat',
                  'CurveVector', 'CurveLinearColor', 'VectorFieldStatic'}
OPTIONS = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_searchable_names=False,
    include_soft_management_references=False, include_hard_management_references=False)


def write(name, value):
    """只保存本次独占目录中的机器证据；完整资产列表不刷屏。"""
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')


def assert_owner():
    """阻止跨编辑器/跨工程误写；每次实际写操作都重新核对。"""
    owner = json.loads((EVIDENCE / 'Editor.json').read_text(encoding='utf-8-sig'))
    assert owner['PID'] == os.getpid(), '当前编辑器不是本轮所有者'
    assert Path(unreal.Paths.get_project_file_path()).resolve() == ROOT/'Game/DivineBeastsArena.uproject'
    assert '-PetalBloomOwnedMigration' in unreal.SystemLibrary.get_command_line()


def source_roots():
    """源项目Game目录无当前工程同名资产时才临时挂载；源插件仅挂载内容，不加载模块。"""
    roots = {f'/{p.stem}/': p.parent/'Content' for p in (SOURCE/'Plugins').glob('*/*.uplugin')}
    roots['/Game/'] = SOURCE/'Content'
    for item in roots['/Game/'].rglob('*.uasset'):
        assert not (ROOT/'Game/Content'/item.relative_to(roots['/Game/'])).exists(), '源Game包与当前工程重名'
    for mount, folder in roots.items():
        unreal.SystemLibrary.execute_console_command(None,
            f'PackageName.RegisterMountPoint {mount} {folder.as_posix()}/')
    return roots


def inspect():
    """按真实文件得到AR类和完整依赖；只纳入效果及视觉资源，演示逻辑另列而不伪造迁移。"""
    assert_owner()
    roots = source_roots()
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous(list(roots), True)
    rows = []
    for mount, folder in roots.items():
        for file in sorted(folder.rglob('*.uasset')):
            package = mount + file.relative_to(folder).with_suffix('').as_posix()
            data = ar.get_asset_by_object_path(package+'.'+file.stem)
            assert data.is_valid(), '源包未被真实AssetRegistry识别：'+package
            cls = str(data.asset_class_path.asset_name)
            dependencies = [str(d) for d in (ar.get_dependencies(package, OPTIONS) or [])]
            relative = file.relative_to(folder).with_suffix('').as_posix()
            # 去掉源库重复根，保留业务分类；避免Windows包路径因嵌套两套库名过长。
            if mount == '/ModularGameVFX/' and relative.startswith('ModularGameVFXLibrary/'):
                relative = relative[len('ModularGameVFXLibrary/'):]
            else:
                relative = mount.strip('/')+'/'+relative
            rows.append(dict(source=package, file=str(file), className=cls,
                selected=cls in VISUAL_CLASSES, dependencies=dependencies,
                target=DEST+'/'+relative,
                sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
    selected = {r['source'] for r in rows if r['selected']}
    external = [{'source':r['source'], 'dependency':d} for r in rows if r['selected']
                for d in r['dependencies'] if d.startswith(tuple(roots)) and d not in selected]
    # 对不在源文件集合中的外部/Game引用明确失败，不能靠运行时访问原库掩盖未迁移。
    write('Plan.json', dict(assets=rows, selectedCount=len(selected), unresolvedSourceReferences=external,
        classCounts=dict(collections.Counter(r['className'] for r in rows if r['selected'])),
        project=unreal.Paths.get_project_file_path(), sourceProject=str(SOURCE)))
    assert not external, '视觉资产依赖未纳入闭包，见Plan.json'
    print('VFX_PLAN', len(selected), dict(collections.Counter(r['className'] for r in rows if r['selected'])))


def migrate():
    """完整映射一次交给原生AdvancedCopy；编辑器保存的目标包才是正式产物。"""
    assert_owner()
    plan = json.loads((EVIDENCE/'Plan.json').read_text(encoding='utf-8'))
    source_roots()
    mapping = {r['source']:r['target'] for r in plan['assets'] if r['selected']}
    assert not plan['unresolvedSourceReferences']
    write('Copy.json', {'status':'running','count':len(mapping)})
    result = unreal.GamePlatformAssetMigrationLibrary.copy_packages_with_references(mapping, DEST)
    # UE Python将bool返回值+OutError投影为“成功时返回字符串，失败时None”，空字符串仍是成功。
    ok, error = result if isinstance(result, tuple) else (result is not None, result or '')
    write('Copy.json', {'status':'saved' if ok else 'failed','count':len(mapping),'error':error})
    assert ok, error
    print('VFX_PACKAGES_SAVED', len(mapping))


def make_definition(path, system, namespace, name, attached=False, lifetime=10.0):
    """逻辑身份、系统、EffectType来自显式输入；不借用源库自定义类或数据播放器。"""
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), '定义已存在，拒绝覆盖：'+path
    cls = unreal.GamePlatformVFXAttachedDefinition if attached else unreal.GamePlatformVFXInstantDefinition
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    obj = unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1], path.rsplit('/',1)[0], cls, factory)
    assert obj is not None
    identity = unreal.GamePlatformId()
    assert identity.import_text(f'(Namespace="{namespace}",Name="{name}",LogicalVersion=1)')
    obj.set_editor_property('logical_id', identity)
    obj.set_editor_property('niagara_system', system)
    obj.set_editor_property('effect_type', system.get_editor_property('effect_type'))
    obj.set_editor_property('enable_scalability', bool(system.get_editor_property('effect_type')))
    obj.set_editor_property('max_lifetime_seconds', lifetime)
    if attached: obj.set_editor_property('default_attach_point', unreal.Name('None'))
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj, False), '定义保存失败：'+path
    return namespace+'.'+name+'@1'


def definitions():
    """为每个完整Niagara系统建立平台定义；治疗额外使用既定语义与受疗者附着合同。"""
    assert_owner()
    plan = json.loads((EVIDENCE/'Plan.json').read_text(encoding='utf-8'))
    entries = []
    for row in plan['assets']:
        if not row['selected'] or row['className'] != 'NiagaraSystem': continue
        system = unreal.load_asset(row['target'])
        assert isinstance(system, unreal.NiagaraSystem)
        suffix = row['source'].rsplit('/',1)[1]+'_'+hashlib.sha256(row['source'].encode()).hexdigest()[:8]
        name = suffix.lower()
        path = DEST+'/Definitions/DA_DBA_Imported_'+suffix
        logical = make_definition(path, system, 'presentation.dba.library', name)
        entries.append(dict(source=row['source'], system=row['target'], definition=path, logicalId=logical))
    petal = [e for e in entries if e['source'].endswith('/NS_Nature_PetalBloom_Aura')]
    assert len(petal) == 1, '花瓣绽放源系统身份不唯一'
    # 治疗属于已有开发魔法装配；该路径已被Data扫描和客户端Cook显式登记，避免全库强制发布。
    healing = '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_HealingPetalBloom'
    make_definition(healing, unreal.load_asset(petal[0]['system']),
        'presentation.dba.development.magic.healing', 'impact', attached=True, lifetime=6.0)
    catalog = dict(entries=entries, healingDefinition=healing, healingSystem=petal[0]['system'],
        ruleZh='运行时由GamePlatformData持有资源租约、GamePlatformPresentation解析、GamePlatformVFX唯一执行；不加载源库Runtime。')
    write('Definitions.json', catalog)
    print('VFX_DEFINITIONS_SAVED',len(entries), 'HEALING_SYSTEM', petal[0]['system'])


def verify():
    """核对全部目标实物、源文件不变和旧挂载/源框架引用；不冒充GPU、Cook或视觉验收。"""
    assert_owner()
    plan = json.loads((EVIDENCE/'Plan.json').read_text(encoding='utf-8'))
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous([DEST], True)
    errors = []
    for row in plan['assets']:
        if hashlib.sha256(Path(row['file']).read_bytes()).hexdigest() != row['sha256']:
            errors.append({'sourceChanged':row['file']})
        if not row['selected']: continue
        target = row['target']
        if not unreal.EditorAssetLibrary.does_asset_exist(target): errors.append({'missing':target})
        for dep in (ar.get_dependencies(target, OPTIONS) or []):
            dep = str(dep)
            if dep.startswith(('/ModularGameVFX/','/ModularGameVFXDemo/','/ModularGameVFXShowcase/',
                               '/Script/ModularGameVFX','/Game/ArcaneMage/','/Game/FireMage/','/Game/FrostMage/')):
                errors.append({'asset':target,'externalDependency':dep})
    write('Verification.json', {'passed':not errors,'errors':errors,
        'assetCount':plan['selectedCount'],'sourcePreserved':not any('sourceChanged' in e for e in errors),
        'gpuCompiled':False,'clientCookVerified':False,'serverStripVerified':False,'manualVisualAccepted':False})
    assert not errors, '迁移依赖或源文件审查失败，见Verification.json'
    print('VFX_MIGRATION_VERIFIED', plan['selectedCount'])


if __name__ == '__main__':
    action = globals().get('VFX_MIGRATION_ACTION', 'inspect')
    {'inspect':inspect, 'migrate':migrate, 'definitions':definitions, 'verify':verify}[action]()
