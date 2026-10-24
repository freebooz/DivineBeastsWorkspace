"""直接Monolith内运行的植物风只读原生审计；仅写Saved证据，不编译/保存/改场景。

audit('PlatformContract'/'ST9'/'Coverage'/'ColdReload')按阶段检查真实资产和连线。
任何缺失项抛异常并保存失败报告；静态接线通过不代表GPU、准入或人工验收通过。
"""
import json
import os
import pathlib
import sys
import unreal as u

ROOT = pathlib.Path(u.Paths.project_dir()).resolve().parent
sys.path.insert(0, str(ROOT / 'Tools/Unreal/Weather'))
import UnifiedVegetationWindContract as contract
import AuthorUnifiedVegetationWindMonolith as author


def function_tree(asset, seen=None):
    """只读遍历实际函数硬引用，限制循环；不会按路径存在推断已连接。"""
    seen = set() if seen is None else seen
    if asset.get_path_name() in seen:
        return []
    seen.add(asset.get_path_name())
    is_function = isinstance(asset, u.MaterialFunction)
    roots = [e for e in author.expressions(asset) if isinstance(e, u.MaterialExpressionFunctionOutput)] if is_function else [
        author.LIB.get_material_property_input_node(asset, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)]
    result, visited = [], set()
    pending = [e for e in roots if e]
    get_inputs = author.LIB.get_inputs_for_material_function_expression if is_function else author.LIB.get_inputs_for_material_expression
    while pending:
        expression = pending.pop()
        if expression.get_path_name() in visited:
            continue
        visited.add(expression.get_path_name()); result.append((asset, expression))
        pending.extend(e for e in get_inputs(asset, expression) if e)
        if isinstance(expression, u.MaterialExpressionMaterialFunctionCall):
            function = expression.get_editor_property('material_function')
            if function:
                result.extend(function_tree(function, seen))
    return result


def has_global_wind(material):
    """母材质及其函数必须实际引用唯一Global函数与平台MPC；不接受独立WindDirection参数。"""
    tree = function_tree(material)
    functions = {a.get_path_name().split('.')[0] for a, _ in tree}
    collections = {e.get_editor_property('collection').get_path_name().split('.')[0]
                   for _, e in tree if isinstance(e, u.MaterialExpressionCollectionParameter)
                   and e.get_editor_property('collection')}
    return contract.GLOBAL_FUNCTION in functions and author.MPC in collections


def protection(scene):
    """材质路径允许本任务改变；Actor/组件身份、模型、变换、实例数量与哈希必须保持。"""
    result = []
    for actor in scene['actors']:
        result.append({'actor': actor['actor'], 'transform': actor['transform'], 'components': [
            {key: c[key] for key in ('component', 'mesh', 'transform', 'instances', 'instance_transform_sha256') if key in c}
            for c in actor['components']]})
    return result


def audit(stage):
    """输出明确的阶段通过/失败和待执行项；未知阶段拒绝，避免空测试固定成功。"""
    if stage not in ('PlatformContract', 'ST9', 'Coverage', 'ColdReload'):
        raise ValueError('未知审计阶段: ' + stage)
    issues = []
    collection = u.load_asset(author.MPC)
    if not collection:
        issues.append('缺少真实平台MPC')
    else:
        scalars = {str(p.get_editor_property('parameter_name')): p.get_editor_property('default_value')
                   for p in collection.get_editor_property('scalar_parameters')}
        vectors = {str(p.get_editor_property('parameter_name')): p.get_editor_property('default_value')
                   for p in collection.get_editor_property('vector_parameters')}
        if len(scalars) != 9 or scalars.get(contract.WIND_SCALAR) != 2026.:
            issues.append('风标量/原八标量合同不完整')
        if set(vectors) != set(contract.WIND_VECTORS):
            issues.append('风From/To/Timing向量合同不完整')
        for name, value in vectors.items():
            if any(getattr(value, axis) != 0. for axis in ('r', 'g', 'b', 'a')):
                issues.append('风向量默认值不为关闭状态: ' + name)
    required = [contract.GLOBAL_FUNCTION, contract.GENERIC_FUNCTION]
    if stage != 'PlatformContract':
        required += [contract.ST9_FUNCTION, contract.ST9_MASTER]
    assets = {}
    for path in required:
        asset = u.load_asset(path)
        if not asset:
            issues.append('缺少原生资产: ' + path)
        else:
            assets[path] = asset
            if u.EditorAssetLibrary.get_metadata_tag(asset, 'UnifiedWindState') != 'GraphAuthored':
                issues.append('资产作者未形成完整图: ' + path)
    if contract.ST9_MASTER in assets:
        master = assets[contract.ST9_MASTER]
        if not has_global_wind(master):
            issues.append('ST9母材质没有真实接入共享风')
        function = assets.get(contract.ST9_FUNCTION)
        if function:
            for expression in author.expressions(function):
                if any(isinstance(e, u.MaterialExpressionTime)
                       for e in author.LIB.get_inputs_for_material_function_expression(function, expression)):
                    issues.append('ST9仍有直连本地Time消费者: ' + expression.get_name())
            guards = [e for e in author.expressions(function) if isinstance(e, u.MaterialExpressionCustom)
                      and e.get_editor_property('code') == contract.FINITE_WEIGHT_HLSL]
            if len(guards) < 3:
                issues.append('ST9数值权重有限值保护缺失')
    if stage in ('Coverage', 'ColdReload'):
        baseline_path = author.EVIDENCE / 'SceneProtection.json'
        scene = author.scene_snapshot()
        if not baseline_path.exists():
            issues.append('缺少最新纯数值场景保护基线')
        elif protection(scene) != protection(json.loads(baseline_path.read_text(encoding='utf-8'))):
            issues.append('场景Actor/组件/实例变换发生变化')
        for actor in scene['actors']:
            for component in actor['components']:
                if not contract.is_plant_mesh(component['mesh']):
                    continue
                mesh = u.load_asset(component['mesh'])
                if mesh.get_editor_property('nanite_settings').get_editor_property('enabled'):
                    issues.append('需额外核对Nanite覆盖材质，当前不宣称已覆盖: ' + component['mesh'])
                paths = set(component['materials'])
                paths.update(slot.get_editor_property('material_interface').get_path_name()
                             for slot in mesh.get_editor_property('static_materials')
                             if slot.get_editor_property('material_interface'))
                for path in paths:
                    material = u.load_asset(path) if path else None
                    parent = material.get_editor_property('parent') if isinstance(material, u.MaterialInstanceConstant) else material
                    if not isinstance(parent, u.Material) or not has_global_wind(parent):
                        issues.append('植物材质漏接: ' + str(path))
    if stage == 'ColdReload':
        saved = json.loads((author.EVIDENCE / 'SavedOwnedAssets.json').read_text(encoding='utf-8'))
        if saved.get('author_pid') == os.getpid() or 'author_pid' not in saved:
            issues.append('必须从独立新Editor进程冷加载，当前PID不能代替独立证据')
        dirty = {p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        if any(path.split('.')[0] in dirty for path in saved['assets']):
            issues.append('冷加载时本任务资产仍为未保存状态')
    report = {'stage': stage, 'status': 'FAIL' if issues else 'PASS', 'issues': sorted(set(issues)),
              'editor_pid': os.getpid(), 'assets_found': sorted(assets),
              'shader_compile_verified': False, 'gpu_verified': False, 'manual_visual_accepted': False}
    author.write('Audit_' + stage + '.json', report)
    print(json.dumps(report, ensure_ascii=False))
    if issues:
        raise RuntimeError('统一风场原生审计未通过: ' + '; '.join(sorted(set(issues))))
    return report
