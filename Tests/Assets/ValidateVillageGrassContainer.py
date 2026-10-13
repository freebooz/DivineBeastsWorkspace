# -*- coding: utf-8 -*-
"""客户端草地发布门禁：只接受真实IoStore ExportBundleData，不用磁盘资源或BulkData替代。
输入UnrealPak清单，三张项目草地必须具有WindowsClient正长度导出。只写指定报告。
失败非零；不表示贴图在屏幕上可见、GPU预算或人工视觉验收通过。
"""
import argparse
import csv
import json
from pathlib import Path

REQUIRED = tuple('DBAPresentationPack_Core/Content/Textures/Terrain/Grass/T_DBA_Village_Grass_' + role + '.uasset'
                 for role in ('BaseColor', 'Normal', 'ORM'))


def validate(path):
    """校验每张贴图恰好一个真实导出；空表、错误类型/平台/大小均失败。"""
    with path.open(encoding='utf-8-sig', newline='') as stream:
        rows = [{k.strip(): str(v).strip() for k, v in row.items()} for row in csv.DictReader(stream)]
    if not rows or not {'Filename', 'ChunkType', 'Platform', 'Size'}.issubset(rows[0]):
        raise ValueError('不是非空IoStore导出清单')
    exports = [row['Filename'].replace('\\', '/').lower() for row in rows
               if row['ChunkType'] == 'ExportBundleData' and row['Platform'] == 'WindowsClient' and int(row['Size']) > 0]
    failures = [p for p in REQUIRED if sum(n.endswith('/' + p.lower()) for n in exports) != 1]
    return {'Passed': not failures, 'RequiredCount': 3, 'MissingOrDuplicate': failures,
            'ContainerCsv': str(path.resolve()), 'ManualVisualAccepted': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='新手村三张客户端草地IoStore门禁')
    parser.add_argument('--csv', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = validate(args.csv)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    if args.report:
        args.report.write_text(payload, encoding='utf-8')
    print(payload)
    raise SystemExit(0 if result['Passed'] else 1)
