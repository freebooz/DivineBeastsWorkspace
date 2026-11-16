"""离线准备风场配置；默认仅输出差异，--apply才写入，须在工程独占窗口执行。

只增补Village包→既有晴天天气ID及审核目录NeverCook；原有其他World映射保留。
原生资产仍须Monolith制作。本工具不构建、不Cook、不启动或关闭任何进程。
"""
import argparse
import json
import pathlib
import re
import sys
import os
import tempfile
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[3]
SECTION = '/Script/DBAWorldsRuntime.DivineBeastsWorldGameMode'
PROPERTY = 'InitialWeatherPresetIdsByWorld'
WORLD = '/DBAWorldPack_Village/Maps/L_Village_Start'
PRESET = 'db.weather.clear@1'
REVIEW = '/DBAWorldPack_Village/Development/Wind'


def commit_updates(root, updates, evidence):
    """预检全部前像、备份并逐文件原子替换；失败只回退仍与本工具写入相同的文件。"""
    prepared, written = [], []
    try:
        for path, before, after in updates:
            path.resolve().relative_to(root.resolve())
            if path.read_bytes() != before:
                raise RuntimeError('配置发生并发修改，拒绝写入: ' + str(path))
            backup = evidence / path.relative_to(root); backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(before)
            descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.wind-', suffix='.tmp', dir=path.parent)
            with os.fdopen(descriptor, 'wb') as target:
                target.write(after)
            prepared.append((path, before, after, pathlib.Path(temporary)))
        for path, before, after, temporary in prepared:
            if path.read_bytes() != before:
                raise RuntimeError('替换前配置被并发修改: ' + str(path))
            os.replace(temporary, path); written.append((path, before, after))
    except BaseException:
        for path, before, after in reversed(written):
            if path.read_bytes() == after:
                descriptor, temporary = tempfile.mkstemp(prefix=path.name + '.wind-rollback-', dir=path.parent)
                with os.fdopen(descriptor, 'wb') as target:
                    target.write(before)
                os.replace(temporary, path)
        raise
    finally:
        for _, _, _, temporary in prepared:
            if temporary.exists(): temporary.unlink()


def configure_ini(text):
    """按精确section合并一份FName→FName映射；未知复杂语法拒绝，绝不抹掉其他映射。"""
    lines = text.splitlines(); active = None; matches = []
    for index, line in enumerate(lines):
        if line.strip().startswith('[') and line.strip().endswith(']'):
            active = line.strip()[1:-1]
        if active and active.casefold() == SECTION.casefold():
            if re.match(r'\s*[+!.-]' + PROPERTY + r'\s*=', line, re.I):
                raise ValueError('天气映射含未支持的INI操作符，拒绝覆盖')
            if re.match(r'\s*' + PROPERTY + r'\s*=', line, re.I):
                matches.append(index)
    if len(matches) > 1:
        raise ValueError('天气映射重复，先审核配置')
    mapping = {}
    if matches:
        raw = lines[matches[0]].split('=', 1)[1].strip()
        pairs = re.findall(r'\(\s*"([^"\\]+)"\s*,\s*"([^"\\]+)"\s*\)', raw)
        if raw != '()' and not pairs:
            raise ValueError('无法安全合并现有天气映射')
        canonical = '(' + ','.join('("' + k + '","' + v + '")' for k, v in pairs) + ')'
        if re.sub(r'\s+', '', raw) != re.sub(r'\s+', '', canonical):
            raise ValueError('现有映射含未解析内容，拒绝覆盖')
        if len(dict(pairs)) != len(pairs):
            raise ValueError('已有重复世界键')
        mapping.update(pairs)
    if WORLD in mapping and mapping[WORLD] != PRESET:
        raise ValueError('新手村已有明确天气选择，保留并要求人工核对: ' + mapping[WORLD])
    mapping[WORLD] = PRESET
    value = PROPERTY + '=(' + ','.join('("' + k + '","' + v + '")' for k, v in sorted(mapping.items())) + ')'
    if matches:
        lines[matches[0]] = value
    else:
        lines.extend(['', '[' + SECTION + ']', '; 统一风只选择既有服务器天气Definition，参数仍归内容包。', value])
    exclusion = '+DirectoriesToNeverCook=(Path="' + REVIEW + '")'
    if exclusion not in lines:
        lines.extend(['', '[/Script/UnrealEd.ProjectPackagingSettings]', '; 独立风审核夹具不进入发行产物。', exclusion])
    newline = '\r\n' if '\r\n' in text else '\n'
    return newline.join(lines) + newline


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=pathlib.Path, default=ROOT)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args(); root = args.workspace.resolve()
    ini = root / 'Game/Config/DefaultGame.ini'
    before = ini.read_bytes(); bom = before.startswith(b'\xef\xbb\xbf')
    after = configure_ini(before.decode('utf-8-sig')).encode('utf-8-sig' if bom else 'utf-8')
    policy_path = root / 'Build/Game/ServerPresentationExclusions.json'
    policy_before = policy_path.read_bytes(); policy = json.loads(policy_before.decode('utf-8-sig'))
    rule = {'PackagePath': REVIEW,
            'SourceDirectory': 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Development/Wind',
            'Profiles': 'AllServers', 'Availability': 'Present',
            'Reason': '统一植物风原生审核地图/夹具不提供权威碰撞或玩法，禁止进入服务器发布包。'}
    existing = [r for r in policy['CookDirectories'] if r['PackagePath'] == REVIEW]
    if existing and existing != [rule]:
        raise ValueError('审核目录已有不同剥离规则，拒绝覆盖')
    if not existing:
        policy['CookDirectories'].append(rule)
    policy_after = (json.dumps(policy, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    changes = [str(p.relative_to(root)) for p, old, new in ((ini, before, after), (policy_path, policy_before, policy_after)) if old != new]
    if args.apply:
        evidence = root / 'Saved/Validation/UnifiedVegetationWind20261024' / str(uuid.uuid4()) / 'ConfigBefore'
        evidence.mkdir(parents=True)
        sys.path.insert(0, str(root / 'Build/Game'))
        from ServerPresentationPolicy import load_policy
        review_directory = root / rule['SourceDirectory']
        created_directory = not review_directory.exists()
        review_directory.mkdir(parents=True, exist_ok=True)
        try:
            candidate = evidence / 'CandidatePolicy.json'; candidate.write_bytes(policy_after)
            # 在任何实际配置写入前，真实校验完整策略及全部派生配置标记/边界。
            checked = load_policy(candidate, root)
            plans = checked.planned_configs(root)
            updates = [(ini, before, after), (policy_path, policy_before, policy_after)]
            updates.extend((root / path, old, new) for path, (old, new) in plans.items())
            commit_updates(root, updates, evidence)
        except BaseException:
            if created_directory and review_directory.is_dir() and not any(review_directory.iterdir()):
                review_directory.rmdir()
            raise
    print(json.dumps({'mode': 'apply' if args.apply else 'preview', 'source_changes': changes,
                      'editor_build_required_after_apply': True, 'native_assets_authored': False}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
