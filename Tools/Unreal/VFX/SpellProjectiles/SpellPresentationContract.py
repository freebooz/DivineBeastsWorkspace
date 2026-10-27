"""当前法术表现唯一合同的纯读取/派生模块；不导入UE、不创建或改动资产。"""
import json
from pathlib import Path

MANIFEST = Path(__file__).with_name('PresentationManifest.json')
ROOT = Path(__file__).resolve().parents[4]
BEGIN = '; BEGIN GENERATED SPELL PRESENTATION COOK - PresentationManifest.json'
END = '; END GENERATED SPELL PRESENTATION COOK'


def load_manifest():
    """读取并拒绝重复身份/路径，避免多份当前映射或含混的覆盖。"""
    data = json.loads(MANIFEST.read_text('utf-8'))
    rows = data['bindings']
    for key in ('definition', 'system', 'logical_id'):
        if len({v[key] for v in rows}) != len(rows):
            raise ValueError('重复合同键: ' + key)
    if set(forbidden_systems(data)) & {v['system'] for v in rows}:
        raise ValueError('当前系统出现在禁止发布名单')
    return data


def binding(spell, stage):
    """按稳定法术及阶段取唯一合同；没有隐式历史路径回退。"""
    return next(v for v in load_manifest()['bindings'] if v['spell'] == spell and v['stage'] == stage)


def forbidden_systems(data=None):
    """迁移来源及更早退休系统均禁止进入新客户端容器；redirector也不作为可发布执行资产。"""
    data = data or json.loads(MANIFEST.read_text('utf-8'))
    return sorted({v['source_system'] for v in data['bindings']} | set(data['retired_systems']))


def container_asset(package):
    """UE包路径转引擎容器后缀；只处理本合同内容插件。"""
    mount, relative = package.lstrip('/').split('/', 1)
    return mount + '/Content/' + relative + '.uasset'


def required_packages(data=None):
    """作者Cook标签、容器门禁共用完整种子；真实依赖仍由UE Cook解析。"""
    data = data or load_manifest()
    return sorted({data['cook_label']} | {v[k] for v in data['bindings'] for k in ('definition', 'system')} |
                  {v['path'] for v in data['resources']} | set(data['auxiliary_systems']) | set(data['auxiliary_definitions']))


def container_required_assets():
    return [container_asset(p) for p in required_packages() if p != load_manifest()['cook_label']]


def compile_systems():
    """同次Cook须提供原生编译输出的系统；不把库存或编辑器报告当Cook证据。"""
    data = load_manifest()
    return sorted({v['system'] for v in data['bindings']} | set(data['auxiliary_systems']))


def cook_config_block():
    """只生成定义种子和当前目录，依赖由原生标签/真实引用纳入，绝不整目录Cook旧系统。"""
    data = load_manifest()
    lines = [BEGIN]
    lines.extend('+DirectoriesToAlwaysCook=(Path="' + p + '")' for p in data['cook_directories'])
    lines.extend('+DirectoriesToNeverCook=(Path="' + p + '")' for p in data['retired_directories'])
    lines.append('[/Script/Engine.AssetManagerSettings]')
    lines.append('-PrimaryAssetTypesToScan=' + data['label_scan']['default_entry'])
    directories = ','.join('(Path="' + p + '")' for p in data['label_scan']['directories'])
    lines.append('+PrimaryAssetTypesToScan=(PrimaryAssetType="PrimaryAssetLabel",AssetBaseClass=/Script/Engine.PrimaryAssetLabel,bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=(' + directories + '),SpecificAssets=("' + data['cook_label'] + '.' + data['cook_label'].rsplit('/', 1)[1] + '"),Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))')
    lines.append('[/Script/UnrealEd.ProjectPackagingSettings]')
    lines.append(END)
    return '\n'.join(lines)


def sync_cook_configs(check=False):
    """同一合同生成两客户端INI；check模式只读检测漂移。"""
    import re
    expected = cook_config_block()
    for profile in ('FrontEndClient', 'VillageDevelopmentClient'):
        path = ROOT / 'Game/Config/Custom' / profile / 'DefaultGame.ini'
        source = path.read_text('utf-8-sig')
        match = re.search(re.escape(BEGIN) + r'.*?' + re.escape(END), source, re.S)
        if check:
            if not match or match.group() != expected:
                raise RuntimeError('生成Cook合同漂移: ' + str(path))
            continue
        source = re.sub(r'^\+DirectoriesToAlwaysCook=\(Path="/DBAPresentationPack_Core/VFX/(?:DevelopmentMagic|IceArrow)"\)\n?', '', source, flags=re.M)
        if match:
            source = re.sub(re.escape(BEGIN) + r'.*?' + re.escape(END), expected, source, flags=re.S)
        else:
            source += '\n[/Script/UnrealEd.ProjectPackagingSettings]\n' + expected + '\n'
        path.write_text(source, 'utf-8')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sync-cook', action='store_true')
    parser.add_argument('--check-cook', action='store_true')
    args = parser.parse_args()
    if args.sync_cook or args.check_cook:
        sync_cook_configs(check=args.check_cook)
    else:
        print(json.dumps(load_manifest(), ensure_ascii=False, indent=2))
