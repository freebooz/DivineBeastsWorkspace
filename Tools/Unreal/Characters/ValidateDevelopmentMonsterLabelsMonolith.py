#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读真实石灵标签合同：Common保留空中立文字槽，UIFont和中文正文由项目客户端事件组合。
Monolith读取保存组件，禁止Common硬引UI包；UIFont字形、运行时装配和实际画面分别验收。
"""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FONT = '/DBAUIPack_Core/UI/Fonts/Development/F_DBA_DevelopmentMonsterLabel'
BP = '/DBAContentPack_Common/Development/Monsters/BP_DBA_AncientStoneSpiritVisual'


def validate():
    """Common没有中文默认值、没有UI包硬引用；不修改组件或自动修复。"""
    spec = importlib.util.spec_from_file_location('casting_transport', ROOT / 'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    protocol = module.transport()
    checks = []
    for name in ('NameLabel', 'LifeLabel'):
        result = protocol.call('blueprint_query', {'action': 'get_component_details', 'params': {'asset_path': BP, 'component_name': name}})
        fields = {entry['name']: entry['value'] for entry in result['properties']}
        checks.append({'Component': name, 'Font': fields['Font'], 'Text': fields['Text'],
                       'Passed': FONT not in fields['Font'] and fields['Text'] in ('', '""', 'INVTEXT("")')})
    passed = all(entry['Passed'] for entry in checks)
    (protocol.EVIDENCE / 'MonsterLabelContract.json').write_text(json.dumps({'Passed': passed, 'Components': checks, 'ManualVisualAccepted': False}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'Checks': checks, 'Evidence': str(protocol.EVIDENCE)}, ensure_ascii=False))
    if not passed:
        raise RuntimeError('Common文字槽仍有默认中文/多余引号或UI包硬引用，不符合事件组合合同')


if __name__ == '__main__':
    validate()
