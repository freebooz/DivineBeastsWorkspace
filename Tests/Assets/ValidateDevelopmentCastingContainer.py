#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读开发客户端IoStore门禁：真实导出数据必须交付施法动作、起手系统和石灵依赖。
输入为锁定UnrealPak实际生成的CSV，不使用磁盘库存替代发布。报告失败非零退出。
此门禁不证明网络、动画播放或视觉质量；Niagara源模块是EditorOnly，不能要求其独立入包。
"""
import argparse
import csv
import json
from pathlib import Path


def required_assets():
    """稳定软引用闭包；新增/改名必须同时调整目录合同与人工审核说明。"""
    result = []
    common = 'DBAContentPack_Common/Content'
    vfx = 'DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting'
    for spell in ('Fireball', 'Frostbolt', 'ArcaneMissiles'):
        for kind in ('AS', 'AM'):
            result.append(f'{common}/Mannequins/DBA/Animations/DevelopmentCasting/{kind}_DBA_Dev{spell}Cast.uasset')
        result.extend([f'{vfx}/Niagara/NS_DBA_Dev{spell}Cast.uasset',
                       f'{vfx}/Definitions/DA_DBA_VFX_Dev{spell}Cast.uasset'])
    result.append(f'{common}/Development/Monsters/BP_DBA_AncientStoneSpiritVisual.uasset')
    result.append('DBAUIPack_Core/Content/UI/Fonts/Development/F_DBA_DevelopmentMonsterLabel.uasset')
    for material in ('Stone', 'EdgeStone', 'JadeGlow'):
        result.append(f'{common}/Development/Monsters/Materials/M_DBA_StoneSpirit{material}.uasset')
    result.extend(['Engine/Content/BasicShapes/Sphere.uasset', 'Engine/Content/BasicShapes/Cube.uasset'])
    return result


def validate(csv_path):
    """大小为正且平台/块类型匹配才算交付；格式错误、空清单和重复输入不隐藏错误。"""
    with csv_path.open(encoding='utf-8-sig', newline='') as stream:
        rows = [{key.strip(): str(value).strip() for key, value in row.items()} for row in csv.DictReader(stream)]
    if not rows or not {'Filename', 'ChunkType', 'Platform', 'Size'}.issubset(rows[0]):
        raise ValueError('不是非空引擎IoStore CSV')
    exports = {row['Filename'].replace('\\', '/').lower() for row in rows
               if row['ChunkType'] == 'ExportBundleData' and row['Platform'] == 'WindowsClient' and int(row['Size']) > 0}
    required = required_assets()
    missing = [path for path in required if not any(name.endswith('/' + path.lower()) for name in exports)]
    return {'Passed': not missing, 'RequiredCount': len(required), 'Missing': missing,
            'ContainerCsv': str(csv_path.resolve()), 'NetworkAccepted': False, 'ManualVisualAccepted': False}


def main():
    """报告仅写调用者指定证据；不删除/修改资源、容器、地图或配置。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = validate(args.csv)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    if args.report:
        args.report.write_text(payload, encoding='utf-8')
    print(payload)
    return 0 if result['Passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
