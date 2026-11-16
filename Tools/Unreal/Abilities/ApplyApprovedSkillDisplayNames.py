#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM优选技能中文显示名批量数据迁移（纯JSON，不运行虚幻编辑器）。

名称来源为用户明确批准的简洁版Excel快照。按稳定AbilityId更新现有V2数据、
V3审稿名称和创角展示标题；不更改机制、数值、技能ID、图片或生产许可。
默认只读检查；--apply先备份，再以字节快照防并发覆盖；--verify验证已同步。
此工具专用于大批量结构化数据变换，不允许修改任意路径、C++或UE二进制资产。
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import os
import re
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
REGISTRY_PATH = 'Docs/Design/ApprovedSkillDisplayNames_V5.json'
V2_PATH = 'Docs/Design/ZodiacEightSlotSkills_V2.json'
V3_ROOT = 'Docs/Design/ZodiacSkillV3Review'
INDEX_PATH = V3_ROOT + '/ReviewIndex.json'
PREVIEW_PATH = V3_ROOT + '/CharacterCreatePreviewContract.json'
HEROES = ('Rat','Ox','Tiger','Rabbit','Dragon','Snake','Horse','Goat','Monkey','Rooster','Dog','Boar')
HERO_PATHS = tuple(V3_ROOT + '/Hero_' + hero + '.json' for hero in HEROES)
ALLOWED_PATHS = (V2_PATH,) + HERO_PATHS + (INDEX_PATH, PREVIEW_PATH)
APPROVAL = 'SSLM-SKILL-NAMES-SIMPLE-V5-20261018'
NAME_STATUS = 'APPROVED_USER_SIMPLE_V5'


def require(ok, message):
    if not ok:
        raise ValueError('SSLM名称迁移拒绝：' + message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def read_json(raw):
    return json.loads(raw.decode('utf-8-sig'))


def encode_json(document, original=b''):
    text = json.dumps(document, ensure_ascii=False, indent=2) + '\n'
    if b'\r\n' in original:
        text = text.replace('\n', '\r\n')
    encoded = text.encode('utf-8')
    return (b'\xef\xbb\xbf' + encoded) if original.startswith(b'\xef\xbb\xbf') else encoded


def registry():
    document = read_json((ROOT / REGISTRY_PATH).read_bytes())
    require(document['approvalId'] == APPROVAL and document['approvalScope'] == 'DISPLAY_NAMES_ONLY',
            '批准身份或范围不是此次名称确认')
    require(document['sourceWorkbookSha256'] == '1e86f4d6bfb17b9bb4262f3f6941bd16c13c2b53f15933ea5254ee41248f62c1',
            '不能改用旧版命名表或未确认来源')
    require(not document['approveGameplay'] and not document['approveEffectImages'] and
            not document['enableProductionCreation'], '不能随改名批准玩法、视觉或生产展示')
    entries = [dict(zip(document['entryColumns'], row)) for row in document['entries']]
    require(len(entries) == 96 and len({r['abilityId'] for r in entries}) == 96, '必须是96个唯一技能身份')
    require(len({r['approvedNameZh'] for r in entries}) == 96, '简洁名称有重名')
    for i, row in enumerate(entries, 7):
        require(re.fullmatch(r'[\u4e00-\u9fff]{2,3}', row['approvedNameZh']), '名称不符合2至3字')
        require(row['sourceCell'] == f'G{i}', '原表行号不连续或被重新排序')
        require(re.fullmatch(r'dba\.ability\.[a-z0-9_]+@1', row['abilityId']), '技能身份格式错误')
    return {r['abilityId']: r for r in entries}


def invariant_projection(path, document):
    """剔除明确允许的名称字段后，其余完整JSON必须逐字段相等，不能只对数值抽样。"""
    value = copy.deepcopy(document)
    value.pop('nameApprovalId', None)
    value.pop('displayNameApprovalSource', None)
    if path == V2_PATH:
        for hero in value['heroes']:
            for row in hero['slots']:
                row.pop('nameZh')
                row.pop('previousNameBeforeSimpleV5', None)
                row.pop('nameApprovalId', None)
    elif path in HERO_PATHS:
        value.pop('reviewUltimateName')
        for row in value['skillRows']:
            current = row.pop('nameZh')
            row.pop('previousNameBeforeSimpleV5', None)
            row.pop('nameApprovalId', None)
            row['status'].pop('skillName')
            # 只允许替换用书名括号明确标注的本技能标题，正文的战斗描述绝不改写。
            row['creatorDescriptionZh'] = row['creatorDescriptionZh'].replace('「' + current + '」', '「__SKILL_TITLE__」')
            row['imageConcept']['shortPromptZh'] = row['imageConcept']['shortPromptZh'].replace('「' + current + '」', '「__SKILL_TITLE__」')
    elif path == INDEX_PATH:
        value.pop('approvedSkillNameCount', None)
        for hero in value['heroRecords']:
            hero.pop('names')
    elif path == PREVIEW_PATH:
        value['approvalGate'].pop('skillName')
    return value


def transform(originals, names):
    """只处理固定15份JSON；任何未知英雄、额外技能或原名冲突都在写盘之前拒绝。"""
    output = {path: copy.deepcopy(data) for path, data in originals.items()}
    v2 = output[V2_PATH]
    ids = {r['abilityId'] for h in v2['heroes'] for r in h['slots']}
    require(ids == set(names), 'V2技能目录与Excel身份集合不一致')
    for hero in v2['heroes']:
        require(len(hero['slots']) == 8, 'V2英雄不是八槽')
        for row in hero['slots']:
            target = names[row['abilityId']]['approvedNameZh']
            row.setdefault('previousNameBeforeSimpleV5', row['nameZh'])
            row['nameZh'] = target
            row['nameApprovalId'] = APPROVAL
    v2['nameApprovalId'] = APPROVAL
    v2['displayNameApprovalSource'] = REGISTRY_PATH
    all_seen = set()
    by_hero = {}
    for path in HERO_PATHS:
        hero = output[path]
        require(hero['heroId'].split('.')[-1] in HEROES and len(hero['skillRows']) == 8, 'V3英雄身份或槽数错误')
        for row in hero['skillRows']:
            identity = row['abilityId']
            require(identity in names and identity not in all_seen, 'V3技能ID缺失或重复：' + identity)
            selected = names[identity]
            old = row['nameZh']
            new = selected['approvedNameZh']
            require(old in (selected['previousV3NameZh'], new), '当前名称有未经本次授权的新变更：' + identity)
            require(identity.startswith('dba.ability.' + hero['heroId'].split('.')[-1].lower() + '_eight_'),
                    'V3技能跨英雄归属')
            require('「' + old + '」' in row['creatorDescriptionZh'], '创建角色说明标题与技能名不一致：' + identity)
            require('「' + old + '」' in row['imageConcept']['shortPromptZh'], '效果图文字配方标题不一致：' + identity)
            if 'previousNameBeforeSimpleV5' in row:
                require(row['previousNameBeforeSimpleV5'] == selected['previousV3NameZh'], '旧名追溯被改写')
            row['previousNameBeforeSimpleV5'] = selected['previousV3NameZh']
            row['creatorDescriptionZh'] = row['creatorDescriptionZh'].replace('「' + old + '」', '「' + new + '」')
            row['imageConcept']['shortPromptZh'] = row['imageConcept']['shortPromptZh'].replace('「' + old + '」', '「' + new + '」')
            row['nameZh'] = new
            row['nameApprovalId'] = APPROVAL
            row['status']['skillName'] = NAME_STATUS
            all_seen.add(identity)
        hero['reviewUltimateName'] = hero['skillRows'][3]['nameZh']
        hero['nameApprovalId'] = APPROVAL
        by_hero[hero['heroId']] = hero
    require(all_seen == set(names), '12份V3定义没有完整覆盖96技能')
    index = output[INDEX_PATH]
    require({h['heroId'] for h in index['heroRecords']} == set(by_hero), '审稿索引英雄集合不一致')
    for item in index['heroRecords']:
        item['names'] = [r['nameZh'] for r in by_hero[item['heroId']]['skillRows']]
    index['approvedSkillNameCount'] = 96
    index['nameApprovalId'] = APPROVAL
    index['displayNameApprovalSource'] = REGISTRY_PATH
    preview = output[PREVIEW_PATH]
    preview['approvalGate']['skillName'] = NAME_STATUS
    preview['nameApprovalId'] = APPROVAL
    preview['displayNameApprovalSource'] = REGISTRY_PATH
    for path in ALLOWED_PATHS:
        require(invariant_projection(path, originals[path]) == invariant_projection(path, output[path]),
                '有不属于名称审批的字段发生改变：' + path)
    return output


def collect():
    names = registry()
    raw = {}
    for path in ALLOWED_PATHS:
        absolute = ROOT / path
        require(absolute.is_file() and not absolute.is_symlink(), '目标缺失或是符号链接：' + path)
        raw[path] = absolute.read_bytes()
    before = {path: read_json(content) for path, content in raw.items()}
    after = transform(before, names)
    changes = {path: encode_json(after[path], raw[path]) for path in ALLOWED_PATHS if before[path] != after[path]}
    return raw, before, after, changes


def atomic_write(path, content):
    """只写入预检过的确定路径，先写临时文件再原子替换；保留原始换行风格。"""
    temp = path.with_name('.' + path.name + '.names-' + uuid.uuid4().hex + '.tmp')
    try:
        temp.write_bytes(content)
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


def apply_changes(raw, before, after, changes):
    if not changes:
        return {'status':'ALREADY_APPLIED','changedFiles':0,'approvedNames':96}
    # 全部预检和快照比较完成后才开始写盘，不能以部分修改冒充本批成功。
    for path in changes:
        require((ROOT / path).read_bytes() == raw[path], '发现并发写入：' + path)
    directory = ROOT / 'Saved/Validation/SkillNamingV5' / (APPROVAL + '-' + uuid.uuid4().hex[:10])
    directory.mkdir(parents=True, exist_ok=False)
    for path in changes:
        backup = directory / path
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_bytes(raw[path])
    written = []
    try:
        for path, content in changes.items():
            require((ROOT / path).read_bytes() == raw[path], '写入前快照已变化：' + path)
            atomic_write(ROOT / path, content)
            written.append(path)
        for path in ALLOWED_PATHS:
            require(read_json((ROOT / path).read_bytes()) == after[path], '写后回读不一致：' + path)
    except BaseException:
        # 只恢复仍等于本次写入内容的路径，绝不覆盖在故障后被其它会话接管的新文件。
        for path in reversed(written):
            if (ROOT / path).read_bytes() == changes[path]:
                atomic_write(ROOT / path, raw[path])
        raise
    report = {
        'approvalId':APPROVAL,'status':'SOURCE_NAMES_APPLIED','approvedNames':96,
        'changedV3Names':sum(a['nameZh'] != b['nameZh'] for path in HERO_PATHS
                             for a,b in zip(before[path]['skillRows'],after[path]['skillRows'])),
        'changedV2Names':sum(a['nameZh'] != b['nameZh'] for ah,bh in zip(before[V2_PATH]['heroes'],after[V2_PATH]['heroes'])
                             for a,b in zip(ah['slots'],bh['slots'])),
        'changedFiles':len(changes),'nonNameFieldsUnchanged':True,
        'nativeAssetsModified':0,'compiled':False,'imagesApproved':False,
        'backupDirectory':directory.relative_to(ROOT).as_posix(),
        'appliedAtUtc':datetime.now(timezone.utc).isoformat(),
        'files':[{'path':p,'beforeSha256':digest(raw[p]),'afterSha256':digest(changes[p])} for p in changes]
    }
    (directory / 'Report.json').write_bytes(encode_json(report))
    (directory.parent / 'LastApplyReport.json').write_bytes(encode_json(report))
    return report


def main():
    parser = argparse.ArgumentParser(description='SSLM用户批准的中文技能名结构化迁移；无UE编译和原生资产写入')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--apply', action='store_true')
    mode.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    raw, before, after, changes = collect()
    if args.apply:
        report = apply_changes(raw, before, after, changes)
    elif args.verify:
        require(not changes, '存在未同步到优选名称的字段，共' + str(len(changes)) + '份文件')
        report = {'status':'VERIFIED','approvedNames':96,'files':len(ALLOWED_PATHS),'noMutation':True}
    else:
        report = {'status':'PLAN_ONLY','approvedNames':96,'filesToUpdate':list(changes),'noMutation':True}
    print(json.dumps(report, ensure_ascii=False, separators=(',',':')))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
