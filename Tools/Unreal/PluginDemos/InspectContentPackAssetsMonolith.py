"""直接Monolith只读盘点20个真实内容挂载点；不加载资源、不播放、不签收。"""
from __future__ import annotations
from collections import Counter
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def inspect_content_packs():
    import unreal
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != (ROOT / 'Game/DivineBeastsArena.uproject').resolve():
        raise RuntimeError('wrong editor project')
    if not str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'):
        raise RuntimeError('requires locked UE5.8')
    author = ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json'
    raw = author.read_bytes()
    document = json.loads(raw.decode('utf-8-sig'))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    rows = []
    for plugin in document['plugins']:
        if plugin['plugin_kind'] != 'content_pack':
            continue
        identity = plugin['plugin_id']
        assets = registry.get_assets_by_path('/' + identity, recursive=True, include_only_on_disk_assets=True)
        native = []
        for asset in assets:
            package = str(asset.package_name)
            if not package.startswith('/' + identity + '/'):
                raise RuntimeError('foreign package returned by content mount: ' + package)
            klass = asset.asset_class_path
            # AssetData两项原生身份组成顶层对象路径；SoftObjectPath的Python str仅输出结构地址。
            native.append({'package': package, 'object_path': package + '.' + str(asset.asset_name),
                           'class': str(klass.package_name) + '.' + str(klass.asset_name),
                           'redirector': asset.is_redirector(), 'loaded_before_inspection': asset.is_asset_loaded(),
                           'native_load': 'not_run', 'consumer_preview': 'not_run', 'human_review': 'pending'})
        rows.append({'plugin_id': identity, 'assets': sorted(native, key=lambda a: a['package']),
                     'count': len(native), 'class_counts': dict(Counter(a['class'] for a in native))})
    result = {'project': str(project), 'source_fingerprint': hashlib.sha256(raw).hexdigest(),
              'state': 'native_registry_only', 'plugin_count': len(rows), 'asset_count': sum(p['count'] for p in rows),
              'plugins': rows, 'native_load': 'not_run', 'runtime': 'not_run', 'human_review': 'pending'}
    target = ROOT / 'Saved/Validation/PluginManualUI20261027/T03.Content.NativeRegistry.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return {'report': str(target), 'plugin_count': len(rows), 'asset_count': result['asset_count']}


if __name__ == '__main__':
    print(json.dumps(inspect_content_packs(), ensure_ascii=False))
