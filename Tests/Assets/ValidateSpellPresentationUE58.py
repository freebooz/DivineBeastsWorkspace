"""独立只读UE5.8当前法术门禁；共享路径合同，独立核对类、ID、schema、范围和Cook种子。

不导入作者、不修改原生资源；compile_first及Cook/GPU/联网/人工验收仍须独立证据。
"""
import json
import math
from pathlib import Path
import sys
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Unreal/VFX/SpellProjectiles'))
from SpellPresentationContract import load_manifest, required_packages


def validate(report_path=None, raise_on_error=True):
    errors, inspected = [], []
    assert Path(u.Paths.get_project_file_path()).resolve() == ROOT / 'Game/DivineBeastsArena.uproject'
    assert u.SystemLibrary.get_engine_version().startswith('5.8.')
    data = load_manifest()
    for row in data['bindings']:
        definition = u.load_asset(row['definition'])
        cls = u.GamePlatformVFXInstantDefinition if row['stage'] == 'Impact' else u.GamePlatformVFXAttachedDefinition
        system = u.load_asset(row['system'])
        if not isinstance(definition, cls) or not isinstance(system, u.NiagaraSystem):
            errors.append('类/资产缺失: ' + row['definition']); continue
        logical = definition.get_editor_property('logical_id')
        actual_id = str(logical.namespace) + '.' + str(logical.name) + '@' + str(logical.logical_version)
        if actual_id != row['logical_id']:
            errors.append('稳定ID变化: ' + row['definition'])
        reference = definition.get_editor_property('niagara_system')
        actual = reference.get_path_name().split('.')[0] if reference else ''
        if actual != row['system']:
            errors.append('当前软引用不符: ' + row['definition'] + ': ' + actual)
        if definition.get_editor_property('quality_variants') or definition.get_editor_property('platform_variants'):
            errors.append('未登记覆盖路径: ' + row['definition'])
        if row['require_cast_parameters']:
            rules = {str(r.name): r for r in definition.get_editor_property('parameter_schema').rules}
            for key, (low, high) in data['cast_parameters'].items():
                rule = rules.get(key)
                if rule is None or rule.type != u.GamePlatformVFXParameterType.FLOAT or not rule.required or rule.min_value > low + .00001 or rule.max_value < high:
                    errors.append('施法必需User schema不符: ' + row['definition'] + ': ' + key)
        bounds = system.get_editor_property('fixed_bounds')
        if not bounds.is_valid or any(not math.isfinite(getattr(bounds.min, a)) or not math.isfinite(getattr(bounds.max, a)) or getattr(bounds.max, a) <= getattr(bounds.min, a) for a in ('x', 'y', 'z')):
            errors.append('系统Bounds无效: ' + row['system'])
        extent = row.get('fixed_extent')
        if extent is not None and any(abs(getattr(bounds.min, a) + extent) > .01 or abs(getattr(bounds.max, a) - extent) > .01 for a in ('x', 'y', 'z')):
            errors.append('独立已批准固定Bounds不符: ' + row['system'])
        inspected.append(dict(definition=row['definition'], system=actual, logical_id=actual_id))
    for row in data['resources'] + data['editor_resources']:
        asset = u.load_asset(row['path'])
        if not isinstance(asset, getattr(u, row['unreal_class'])):
            errors.append('独立资源完整性/类型错误: ' + row['path'])
        elif 'srgb' in row and asset.get_editor_property('srgb') != row['srgb']:
            errors.append('RGB/线性遮罩合同错误: ' + row['path'])
    for path in data['auxiliary_systems']:
        if not isinstance(u.load_asset(path), u.NiagaraSystem):
            errors.append('独有辅助系统缺失: ' + path)
    label = u.load_asset(data['cook_label'])
    if not isinstance(label, u.PrimaryAssetLabel):
        errors.append('当前显式Cook标签缺失: ' + data['cook_label'])
    else:
        actual = {a.get_path_name().split('.')[0] for a in label.get_editor_property('explicit_assets')}
        expected = set(required_packages(data)) - {data['cook_label']}
        if actual != expected:
            errors.append('Cook标签种子与合同不一致')
        if label.get_editor_property('rules').get_editor_property('CookRule') != u.PrimaryAssetCookRule.ALWAYS_COOK:
            errors.append('Cook标签未设AlwaysCook')
        if label.get_editor_property('is_runtime_label') or label.get_editor_property('label_assets_in_my_directory'):
            errors.append('Cook标签须只显式收集且自身不进入运行包')
    result = dict(Passed=not errors, Errors=errors, Inspected=inspected, ShaderCompileVerified=False,
                  ClientCookVerified=False, ServerCookVerified=False, GPUVisualVerified=False,
                  NetworkVerified=False, ManualVisualAccepted=False)
    if report_path:
        output = Path(report_path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2), 'utf-8')
    if errors and raise_on_error:
        raise RuntimeError('当前法术原生门禁失败: ' + '; '.join(errors))
    return result


if __name__ == '__main__':
    print(json.dumps(validate(ROOT / 'Saved/Validation/SingleImplementationConvergence20261027/SpellPresentation/NativeIndependent.json'), ensure_ascii=False))
