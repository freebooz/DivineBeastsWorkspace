"""把已核对的内容资源身份编译为唯一作者源的增量候选。

只写Saved下草稿，不自动发布、不标wired、不签收。无具体本组原生资源证据时保留缺口，
不能从同包随便挑一个资源冒充该功能；注册/撤销和世界消费仍需专门真实接线。
"""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = ROOT / 'Saved/Validation/PluginManualUI20261027'
SAMPLE = '/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoAssetPreview.WBP_DBA_PluginDemoAssetPreview_C'
AUTO_CLASSES = {'/Script/Engine.Texture2D', '/Script/Engine.StaticMesh', '/Script/Engine.SkeletalMesh',
                '/Script/Engine.Material', '/Script/Engine.MaterialInstanceConstant'}


def compile_content_cases(document, inventory, native):
    features = {f['feature_id']: f for f in document['features']}
    declared = {p['plugin_id'] for p in document['plugins'] if p['plugin_kind'] == 'content_pack'}
    native_by_owner = {}
    for plugin in native['plugins']:
        owner = plugin['plugin_id']
        if owner not in declared:
            raise ValueError('native owner is not a declared content pack: ' + owner)
        native_by_owner[owner] = {}
        for asset in plugin['assets']:
            if not asset['package'].startswith('/' + owner + '/') or not asset['object_path'].startswith(asset['package'] + '.'):
                raise ValueError('native asset ownership mismatch: ' + asset['object_path'])
            if not asset['redirector']:
                native_by_owner[owner][asset['object_path']] = asset
    scenarios, reviews, pending = [], [], []
    for plugin in inventory['plugins']:
        if plugin['kind'] != 'content':
            continue
        owner = plugin['pluginId']
        for group in plugin['featureGroups']:
            feature = group['featureId']
            if feature not in features or features[feature]['plugin_id'] != owner:
                raise ValueError('feature must exist in sole author source: ' + feature)
            paths = {e.get('objectPathCandidate') for e in group['evidence']}
            candidates = [asset for path, asset in native_by_owner.get(owner, {}).items() if path in paths]
            if not candidates:
                pending.append({'feature_id': feature, 'reason': '本组未提供可匹配的真实Registry资源；需要专门操作适配，不借用其他组资源。'})
                continue
            candidates.sort(key=lambda asset: (asset['class'] not in AUTO_CLASSES, asset['object_path']))
            asset = candidates[0]
            default_mode = 'Auto' if asset['class'] in AUTO_CLASSES else ('Inspect' if asset['class'] == '/Script/Engine.World' else 'NativeEditor')
            identity = feature + '.AssetInspection'
            def parameter(name, kind, value, unit, **bounds):
                return {'parameter_id': name, 'type': kind, 'default_value': value, 'unit': unit,
                        'default_source': '当前原生AssetRegistry及ContentAsset适配显示合同', 'writable': True, 'apply_when': 'execute', **bounds}
            scenario = {'scenario_id': identity, 'feature_id': feature, 'title': group['title'] + '：资源选择与预览',
                'related_feature_ids': [], 'adapter_id': 'ContentAsset', 'template': 'AssetGallery',
                'run_mode': 'editor' if default_mode == 'NativeEditor' else 'local',
                'targets': ['Editor'] if default_mode == 'NativeEditor' else ['Editor', 'Client', 'Game'],
                'required_plugins': [owner, 'DBAClient', 'GamePlatformData', 'DBAUIPack_Core'],
                'preconditions': ['显式开发演示模式、对应内容包启用、真实Monolith预览样板已加载',
                    '资源选择限本包顶层对象；Auto显示纹理/模型/材质，NativeEditor使用原生只读预览',
                    '三维样板仅独立本地World；世界进入和内容注册生命周期另有案例，不由本检查冒充'],
                'map_asset': '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub', 'widget_asset': SAMPLE,
                'definition_assets': [], 'operations': ['Execute', 'Stop', 'Reset'],
                'prepare_timeout_seconds': 30, 'stop_timeout_seconds': 10,
                'parameters': [parameter('Asset', 'asset_path', asset['object_path'], '本包原生顶层对象路径'),
                    parameter('PreviewMode', 'identifier', default_mode, 'Auto / Inspect / NativeEditor'),
                    parameter('PixelSize', 'integer', 64, '像素；图标分别核对48与64', minimum=16, maximum=512),
                    parameter('DistanceCm', 'number', 400, '厘米；仅独立三维样板', minimum=100, maximum=2000),
                    parameter('YawDegrees', 'number', 0, '度；仅独立三维样板', minimum=-180, maximum=180)],
                'review_case_ids': [identity + '.Review']}
            scenario['required_plugins'] = list(dict.fromkeys(scenario['required_plugins']))
            scenarios.append(scenario)
            reviews.append({'review_case_id': identity + '.Review', 'version': 1, 'scenario_id': identity, 'feature_id': feature,
                'steps': ['从本包Registry列表选择实际资源路径，显式设置Asset与PreviewMode并执行',
                    group['manualOperation'], '图标分别输入48和64像素，观察原色、透明度及实际尺寸；声音/动画在原生预览中显式播放',
                    '输入其他包路径、缺失路径和非法模式，确认具体拒绝且不串资源',
                    '执行中停止、切资源后再次执行、Reset并退出World，确认自身请求、对象、租约清理；已有Editor窗口不被误关'],
                'expected_result': '真实选中资源被加载并按支持模式显示；受理、显示、原生预览、完整消费和人工结论分别记录。' + group['normalExpected'],
                'required_evidence': ['screenshot', 'log']})
    return {'scenarios': scenarios, 'review_cases': reviews, 'pending': pending,
            'publish_status': 'draft_requires_adapter_compile_and_monolith_widget', 'human_review': 'not_reviewed'}


def main():
    document = json.loads((ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
    inventory = json.loads((EVIDENCE / 'ProjectContent.Inventory.json').read_text(encoding='utf-8-sig'))
    native = json.loads((EVIDENCE / 'T03.Content.NativeRegistry.json').read_text(encoding='utf-8-sig'))
    result = compile_content_cases(document, inventory, native)
    # 先在内存合并，复用真实作者验证器校验双向关系；不会改生产manifest。
    drafted_ids = {s['scenario_id'] for s in result['scenarios']}
    drafted_reviews = {r['review_case_id'] for r in result['review_cases']}
    document['scenarios'] = [s for s in document['scenarios'] if s['scenario_id'] not in drafted_ids]
    document['review_cases'] = [r for r in document['review_cases'] if r['review_case_id'] not in drafted_reviews]
    for scenario in result['scenarios']:
        feature = next(f for f in document['features'] if f['feature_id'] == scenario['feature_id'])
        if scenario['scenario_id'] not in feature['scenario_ids']:
            feature['scenario_ids'].append(scenario['scenario_id'])
        feature['integration_status'] = 'registered'
    document['scenarios'].extend(result['scenarios']); document['review_cases'].extend(result['review_cases'])
    spec = importlib.util.spec_from_file_location('content_preview_validator', ROOT / 'Tests/Architecture/ValidatePluginDemoManifest.py')
    validator = importlib.util.module_from_spec(spec); spec.loader.exec_module(validator)
    issues = validator.validate_manifest(document, ROOT)
    if issues:
        raise ValueError(json.dumps(issues, ensure_ascii=False))
    output = EVIDENCE / 'T03.ContentCases.Draft.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'draft': str(output), 'scenarios': len(result['scenarios']), 'pending_specialized_groups': len(result['pending']),
                      'published': False, 'wired': False, 'native_run': False, 'human_review': False}, ensure_ascii=True))


if __name__ == '__main__':
    main()
