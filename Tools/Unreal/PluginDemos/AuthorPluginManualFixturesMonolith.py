"""直接Monolith执行的原生人工测试夹具作者；不构建、不联网、不授予正式角色能力。

输入为已审阅的精确资源配方。只制作DBAUIPack_Core/Development内8个资源，
保留已有脏包及共享Manny骨架，保存前备份。原生调用/保存不代替运行或人工验收。
"""
from __future__ import annotations
import json
import os
import shutil
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RECIPE = ROOT / 'Docs/Implementation/PluginDemos/ManualFixtureRecipes.json'
PREFIX = '/DBAUIPack_Core/Development/PluginDemos/Fixtures/'
EVIDENCE = ROOT / 'Saved/Validation/PluginManualUI20261027'


def author_fixtures():
    import unreal
    recipe = json.loads(RECIPE.read_text(encoding='utf-8-sig'))
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != ROOT / 'Game/DivineBeastsArena.uproject' or not unreal.SystemLibrary.get_engine_version().startswith('5.8.'):
        raise RuntimeError('需要当前工程及锁定UE5.8')
    paths = [a['asset'] for a in recipe['input']['actions']] + [recipe['input']['contexts'][0]['context'], recipe['input']['asset'], recipe['flow']['asset'], recipe['equipment']['asset']]
    if len(paths) != 8 or len(set(paths)) != 8 or any(not p.startswith(PREFIX) or p.rsplit('/', 1)[-1].split('.')[0] != p.split('.')[-1] for p in paths):
        raise ValueError('夹具只能使用精确开发目录的8个独立顶层资源')
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if dirty.intersection({p.split('.')[0] for p in paths}):
        raise RuntimeError('目标夹具有未保存编辑；保留并停止')
    # 只在瞬态未注册组件核对既有骨骼；不更改共享模型/骨架，不向游戏World生成对象。
    avatar = unreal.EditorAssetLibrary.load_asset(recipe['equipment']['avatar_mesh'])
    if not isinstance(avatar, unreal.SkeletalMesh):
        raise RuntimeError('真实Manny Avatar不存在')
    component = unreal.new_object(unreal.SkeletalMeshComponent)
    component.set_skeletal_mesh_asset(avatar)
    if not component.does_socket_exist(recipe['equipment']['socket_name']):
        raise RuntimeError('真实Avatar缺少hand_r骨骼/插槽')
    manifest = json.loads((ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
    pack = (ROOT / next(p['descriptor_path'] for p in manifest['plugins'] if p['plugin_id'] == 'DBAUIPack_Core')).parent / 'Content'
    backup = EVIDENCE / 'Backups' / datetime.now().strftime('Fixtures-%Y%m%d-%H%M%S-%f')
    for path in paths:
        filename = pack / Path(path.split('.')[0].removeprefix('/DBAUIPack_Core/')).with_suffix('.uasset')
        if filename.is_file():
            dest = backup / filename.relative_to(pack); dest.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(filename, dest)
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    saved = []

    def obtain(path, klass, factory=None):
        package = path.split('.')[0]
        asset = assets.load_asset(package) if assets.does_asset_exist(package) else None
        if asset is not None and not isinstance(asset, klass):
            raise RuntimeError('已有资产类型不匹配，拒绝覆盖：' + path)
        if asset is None:
            if factory is None:
                factory = unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class', klass)
            directory, _, name = package.rpartition('/')
            asset = tools.create_asset(name, directory, klass, factory)
        if not isinstance(asset, klass):
            raise RuntimeError('原生创建失败：' + path)
        return asset

    def save(asset):
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('原生保存失败：' + asset.get_path_name())
        saved.append({'asset': asset.get_path_name(), 'class': asset.get_class().get_path_name()})

    def tag(text):
        value = unreal.GameplayTag()
        if not value.import_text('(TagName="' + text + '")'):
            raise RuntimeError('原生标签无效：' + text)
        return value

    def key(text):
        value = unreal.Key()
        if not value.import_text(text):
            raise RuntimeError('原生键名无效：' + text)
        return value

    action_values = {}; action_defs = []
    units = {'NormalizedAxis': unreal.GamePlatformInputUnit.NORMALIZED_AXIS, 'Boolean': unreal.GamePlatformInputUnit.BOOLEAN}
    policies = {'MoveAxis': unreal.GamePlatformInputValuePolicy.MOVE_AXIS, 'Passthrough': unreal.GamePlatformInputValuePolicy.PASSTHROUGH}
    for entry in recipe['input']['actions']:
        action = obtain(entry['asset'], unreal.InputAction, unreal.InputAction_Factory())
        value_type = getattr(unreal.InputActionValueType, entry['value_type'].upper())
        action.set_editor_property('value_type', value_type)
        action.set_editor_property('triggers', []); action.set_editor_property('modifiers', [])
        save(action); action_values[action.get_name()] = action
        descriptor = entry['descriptor']
        semantic = unreal.GamePlatformInputSemanticDescriptor(
            semantic_id=unreal.GamePlatformInputSemanticId(tag=tag(descriptor['semantic_id_tag'])),
            unit=units[descriptor['unit']], value_type=value_type, channel_mask=descriptor['channel_mask'], value_policy=policies[descriptor['value_policy']])
        # 该旧结构只有EditDefaultsOnly、未公开BP字段，Python构造器不接受关键字。
        # 使用引擎结构ImportText写真实默认值；文本由封闭配方及原生export_text形成。
        definition = unreal.GamePlatformInputActionDefinition()
        if not definition.import_text('(Descriptor=' + semantic.export_text() + ',Action="' + action.get_path_name() + '")'):
            raise RuntimeError('输入动作结构无法导入：' + action.get_path_name())
        action_defs.append(definition)
    context = obtain(recipe['input']['contexts'][0]['context'], unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    mappings = []
    for entry in recipe['input']['mappings']:
        mapping = unreal.EnhancedActionKeyMapping(action=action_values[entry['action']], key=key(entry['key']))
        modifiers = []
        for token in entry['modifiers']:
            if token['class'] == '/Script/EnhancedInput.InputModifierSwizzleAxis' and token['order'] == 'YXZ':
                modifier = unreal.new_object(unreal.InputModifierSwizzleAxis, outer=context)
                modifier.set_editor_property('order', unreal.InputAxisSwizzle.YXZ)
            elif token['class'] == '/Script/EnhancedInput.InputModifierNegate':
                modifier = unreal.new_object(unreal.InputModifierNegate, outer=context)
                for axis in ('x', 'y', 'z'):
                    modifier.set_editor_property(axis, token['b_' + axis])
            else:
                raise ValueError('未登记的映射修饰器：' + repr(token))
            modifiers.append(modifier)
        mapping.set_editor_property('modifiers', modifiers)
        if 'player_mappable_key_settings' in entry:
            data = entry['player_mappable_key_settings']
            settings = unreal.new_object(unreal.PlayerMappableKeySettings, outer=context)
            for field in ('name', 'display_name', 'display_category'):
                settings.set_editor_property(field, data[field])
            mapping.set_editor_property('setting_behavior', unreal.PlayerMappableKeySettingBehaviors.OVERRIDE_SETTINGS)
            mapping.set_editor_property('player_mappable_key_settings', settings)
        mappings.append(mapping)
    mapping_data = context.get_editor_property('default_key_mappings')
    mapping_data.set_editor_property('mappings', mappings)
    context.set_editor_property('default_key_mappings', mapping_data)
    save(context)
    profile = obtain(recipe['input']['asset'], unreal.GamePlatformInputProfileDefinition)

    def identity(asset, data):
        logical = data['logical_id']
        asset.set_editor_property('logical_id', unreal.GamePlatformId(namespace=logical['namespace'], name=logical['name'], logical_version=logical['version']))
        asset.set_editor_property('data_version', unreal.GamePlatformDataVersion(**data['data_version']))
        asset.set_editor_property('required_definitions', [])

    identity(profile, recipe['input'])
    profile.set_editor_property('actions', action_defs)
    context_definition = unreal.GamePlatformInputContextDefinition()
    if not context_definition.import_text('(Name=Gameplay,Context="' + context.get_path_name() + '",bAllowSharing=False)'):
        raise RuntimeError('输入映射上下文结构无法导入')
    profile.set_editor_property('contexts', [context_definition])
    for field, value in recipe['input']['profile_settings'].items():
        profile.set_editor_property(field, value)
    save(profile)
    flow = obtain(recipe['flow']['asset'], unreal.GamePlatformFlowDefinition); identity(flow, recipe['flow'])
    flow.set_editor_property('entry_node_id', recipe['flow']['entry_node_id'])
    flow.set_editor_property('nodes', [unreal.GamePlatformFlowNodeDefinition(**node) for node in recipe['flow']['nodes']])
    flow.set_editor_property('allow_cycles', False); save(flow)
    equipment = obtain(recipe['equipment']['asset'], unreal.GamePlatformEquipmentVisualDefinition)
    equipment.set_editor_property('equipment_visual_id', 'ManualVisual')
    equipment.set_editor_property('visual_type', unreal.GamePlatformEquipmentVisualType.STATIC_MESH)
    equipment.set_editor_property('static_mesh', assets.load_asset(recipe['equipment']['static_mesh']))
    equipment.set_editor_property('socket_name', recipe['equipment']['socket_name'])
    equipment.set_editor_property('relative_transform', unreal.Transform(scale=[0.2, 0.2, 0.2]))
    equipment.set_editor_property('material_overrides', []); save(equipment)
    result = {'editor_pid': os.getpid(), 'saved_epoch': time.time(), 'saved_count': len(saved), 'assets': saved,
              'avatar_mesh': avatar.get_path_name(), 'hand_r_verified': True, 'backup': str(backup),
              'cold_reload': 'pending', 'runtime': 'not_run', 'human_review': 'pending'}
    (EVIDENCE / 'T04.Fixtures.Authored.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return result


if __name__ == '__main__':
    print(json.dumps(author_fixtures(), ensure_ascii=False))
