"""有限资源归属合同；作者、原生核验及容器门禁共同读取，不实现运行时资源服务。"""
import json
from pathlib import Path
import re

MANIFEST = Path(__file__).with_name('ResourceOwnershipManifest.json')
GROUPS = {
    'plants_runtime': ('/Game/PN_OpenWorldFoliage/', '/DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage/'),
    'plants_samples': ('/Game/PN_OpenWorldFoliage/', '/Game/Development/Vendor/PN_OpenWorldFoliage/'),
    'terrain_review': ('/DBAPresentationPack_Core/Materials/Terrain/', '/DBAWorldPack_Village/Development/Terrain/Production10/'),
    'terrain_color': ('/DBAPresentationPack_Core/Materials/Terrain/TexturesV3/', '/DBAPresentationPack_Core/Textures/Terrain/Production10/'),
    'bird_history': ('/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Meshes/', '/DBAPresentationPack_Core/Development/History/BirdFlock/Meshes/'),
    'stylehex_samples': ('/Game/StyleHex_Studio/', '/Game/Development/Vendor/StyleHex_Studio/'),
}
PACKAGE = re.compile(r'/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+\Z')


def load_contract(path=MANIFEST):
    """读取既有清单并完整验证；未知版本、越界与同名目标失败关闭。"""
    doc = json.loads(Path(path).read_text(encoding='utf-8-sig'))
    validate_contract(doc)
    return doc


def validate_contract(doc):
    """返回有限move列表；只支持本轮已批准Owner，不从输入推导任意根目录。"""
    if doc.get('schema_version') != 1 or doc.get('migration_id') != 'ResourceOwnership20261031':
        raise ValueError('未知资源归属合同身份/版本')
    moves = doc.get('moves')
    if not isinstance(moves, list) or not moves:
        raise ValueError('迁移清单不能为空')
    sources, targets = set(), set()
    for move in moves:
        source, target = move.get('source'), move.get('target')
        if not isinstance(source, str) or not PACKAGE.fullmatch(source) or not isinstance(target, str) or not PACKAGE.fullmatch(target):
            raise ValueError('非法原生包路径')
        if source.casefold() in sources or target.casefold() in targets:
            raise ValueError('重复源或目标冲突')
        sources.add(source.casefold()); targets.add(target.casefold())
    if sources & targets:
        raise ValueError('源和目标发生交叉冲突')
    for move in moves:
        group = move.get('group')
        if group not in GROUPS:
            raise ValueError('未知归属组')
        source_prefix, target_prefix = GROUPS[group]
        if not move['source'].startswith(source_prefix) or not move['target'].startswith(target_prefix):
            raise ValueError('越过已批准Owner归属')
        if move['target'].rsplit('/', 1)[1] != move['source'].rsplit('/', 1)[1]:
            raise ValueError('本次只迁Owner，不能修改资产叶子名')
        if group == 'bird_history' and '/Meshes/Flight/' in move['source']:
            raise ValueError('当前Flight资源不得作为历史迁移')
    return moves


def preflight_paths(doc, existing_sources, existing_targets):
    """写入前检查目标冲突及缺源；集合只表示原生真实资源，不接受重定向冒充源。"""
    moves = validate_contract(doc)
    targets = {p.casefold() for p in existing_targets}
    if any(m['target'].casefold() in targets for m in moves):
        raise ValueError('目标已存在，拒绝覆盖')
    sources = {p.casefold() for p in existing_sources}
    missing = [m['source'] for m in moves if m['source'].casefold() not in sources]
    if missing:
        raise ValueError('缺少原生源资产：'+', '.join(missing[:3]))


def ownership_issues(doc, edges):
    """检查已保存原生包图，不用文件夹存在代替依赖核验。"""
    validate_contract(doc)
    issues = []
    for source, target in set(map(tuple, edges)):
        if source.startswith('/DBAFoliagePack_Core/') and target.startswith('/Game/'):
            issues.append({'kind':'PlantsToHost', 'source':source, 'target':target})
        elif source.startswith('/DBAPresentationPack_Core/') and target.startswith('/DBAWorldPack_'):
            issues.append({'kind':'SharedToWorld', 'source':source, 'target':target})
    return sorted(issues, key=lambda x:(x['source'], x['target']))


def appearance_signature(record):
    """导出PNG哈希已覆盖源像素及尺寸；流送驻留Mip大小和OBJ导出路径不是资源内容。"""
    result = dict(record)
    if isinstance(result.get('leaf'), str):
        result['leaf'] = result['leaf'].casefold()  # UE FName身份不区分大小写，保留磁盘原拼写。
    result.pop('geometryOBJ', None)
    if result.get('class') == '/Script/Engine.Texture2D' and result.get('pixelPNG'):
        result.pop('size', None)
    return result
