"""登记每个插件独立封面的来源哈希及原生UI纹理目标，不创建UE资产。"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).parent
cards = json.loads((HERE / 'PluginHubCards.json').read_text(encoding='utf-8'))['Cards']
files = []
for card in cards:
    name = card['PluginId'].replace('_', '')
    source = HERE / 'Art' / (name + '.png')
    if not source.is_file():
        raise RuntimeError('Missing cover: ' + str(source))
    files.append({'PluginId': card['PluginId'], 'Role': 'Color',
        'Source': source.relative_to(ROOT).as_posix(),
        'Destination': '/DBAUIPack_Core/Development/PluginDemos/Textures/Plugins/TPluginHub' + name,
        'SHA256': hashlib.sha256(source.read_bytes()).hexdigest()})
assert len(files) == len({f['SHA256'] for f in files}), 'Duplicate cover image'
manifest = {'Schema': 1, 'Purpose': 'PluginHubIndependentCoverArt',
    'ArtStandard': 'Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md',
    'TextureStandard': 'Docs/Implementation/UITextureVisualPolicy.md',
    'UIRoots': ['/DBAUIPack_Core/Development/PluginDemos/Textures/'], 'Files': files}
(HERE / 'PluginHubImages.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('Independent covers:', len(files), 'Unique hashes:', len({f['SHA256'] for f in files}))
