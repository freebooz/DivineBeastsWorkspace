"""核对Monolith读取的真实技能栏八个主动原子的鼠标命中，不重建控件树。"""
import json
import sys
from pathlib import Path

NAMES = ('PrimaryAbilitySlot', 'ActiveAbilitySlot1', 'ActiveAbilitySlot2',
         'UltimateAbilitySlot', 'MovementAbilitySlot', 'DefenseAbilitySlot',
         'ControlAbilitySlot', 'SignatureAbilitySlot')


def validate(tree):
    nodes = {node['name']: node for node in tree['root']['children']}
    assert tree['asset_path'] == '/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar'
    failures = []
    for name in NAMES:
        node = nodes.get(name)
        if not node or node['class'] != 'WBP_DBA_UI_AbilitySlot_C' or node['visibility'] != 'ESlateVisibility::Visible':
            failures.append(name)
    assert not failures, '主动技能原子必须自身接收鼠标：' + ', '.join(failures)
    return {'passed': True, 'hit_targets': len(NAMES)}


if __name__ == '__main__':
    print(json.dumps(validate(json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))), ensure_ascii=False))
