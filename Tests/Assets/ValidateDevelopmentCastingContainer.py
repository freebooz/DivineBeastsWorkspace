#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读开发客户端IoStore与GPU编译门禁：施法动作、起手和新版寒冰箭必须完整交付。
输入为锁定UnrealPak实际生成的CSV及同次UAT Cook日志，不使用磁盘库存替代发布。
GPU HLSL错误可能被UE记作Warning并仍返回0，故逐条阻断Niagara/ShaderCompiler诊断。
报告失败非零退出；不证明网络、动画播放或视觉质量；EditorOnly源模块不要求独立入包。
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re


import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Unreal/VFX/SpellProjectiles'))
from SpellPresentationContract import (compile_systems, container_asset, container_required_assets,
                                       forbidden_systems)


def required_assets():
    """当前法术唯一合同+独立动画合同；不维护第二份历史VFX正确版本列表。"""
    result = container_required_assets()
    common = 'DBAContentPack_Common/Content/Mannequins/DBA/Animations/DevelopmentCasting'
    for spell in ('Fireball', 'Frostbolt', 'ArcaneMissiles'):
        for kind in ('AS', 'AM'):
            result.append(f'{common}/{kind}_DBA_Dev{spell}Cast.uasset')
    return result


def validate_exports(exports):
    """明确拒绝旧系统包；完整新闭包也不能掩盖旧包同时发布。"""
    required = required_assets()
    missing = [path for path in required if not any(name.endswith('/' + path.lower()) for name in exports)]
    forbidden = [container_asset(path) for path in forbidden_systems()
                 if any(name.endswith('/' + container_asset(path).lower()) for name in exports)]
    return {'RequiredCount': len(required), 'Missing': missing, 'Forbidden': forbidden}


def validate_cook_shaders(csv_path, cook_log_path=None):
    """只接受CSV所属同次Cook目录的日志，缺失/歧义/未完成/阶段缺证/编译诊断均失败。

    自动查找保持既有CookFrontEndClient调用兼容；显式日志仍必须位于同次UATLogs，
    防止用旧Cook或编辑器compile_first的成功记录覆盖当前发布失败。读取不修改引擎输出。
    """
    log_directory = csv_path.resolve().parent.parent / 'UATLogs'
    candidates = ([cook_log_path.resolve()] if cook_log_path is not None
                  else sorted(log_directory.glob('Cook-*.txt')))
    errors = []
    if len(candidates) != 1:
        errors.append('同次Cook日志必须唯一，实际数量：' + str(len(candidates)))
    log_path = candidates[0] if len(candidates) == 1 else None
    if log_path is not None and (log_path.parent != log_directory or not log_path.is_file()):
        errors.append('Cook日志不属于当前容器的UATLogs或文件不存在')
    result = {'Passed': False, 'CookLog': str(log_path) if log_path else None,
              'Errors': errors, 'ShaderDiagnostics': [], 'CompiledStages': [],
              'MissingStages': compile_systems(), 'CookCompleted': False}
    if errors:
        return result
    raw = log_path.read_bytes()
    result['CookLogSha256'] = hashlib.sha256(raw).hexdigest()
    lines = raw.decode('utf-8-sig', errors='replace').splitlines()
    # 编译条目需来自Niagara实际系统编译输出，路径库存或diagnostics JSON均不能冒充。
    compiled = set()
    compiled_pattern = re.compile(
        r'LogNiagara:\s+Compiling System NiagaraSystem (/[A-Za-z0-9_/]+)\.[A-Za-z0-9_]+ took ')
    shader_diagnostics = re.compile(
        r'LogNiagara:\s+(?:Warning:|Error:|Display: There were issues for compilation)|'
        r'LogShaderCompilers?:\s+(?:Warning:|Error:)|'
        r'(?:error [A-Z]\d{4}:|error: use of undeclared identifier|Failed to compile.*shader)',
        re.IGNORECASE)
    for number, line in enumerate(lines, 1):
        match = compiled_pattern.search(line)
        if match:
            compiled.add(match.group(1))
        if shader_diagnostics.search(line):
            result['ShaderDiagnostics'].append({'Line': number, 'Message': line})
    result['CompiledStages'] = [stage for stage in compile_systems() if stage in compiled]
    result['MissingStages'] = [stage for stage in compile_systems() if stage not in compiled]
    # 总结的0 error不够；必须同时看到所有包完成，避免截断/中止日志被放行。
    result['CookCompleted'] = any('Packages Remain 0 Total ' in line for line in lines) and any(
        re.search(r'LogInit:\s+Display: Success - 0 error\(s\)', line) for line in lines)
    if result['MissingStages']:
        errors.append('缺少实际当前法术系统编译证据：' + ','.join(result['MissingStages']))
    if not result['CookCompleted']:
        errors.append('Cook日志没有完整包完成及成功终态证据')
    if result['ShaderDiagnostics']:
        errors.append('Cook存在Niagara/GPU着色器编译诊断，禁止发布')
    result['Passed'] = not errors
    return result


def validate(csv_path, cook_log_path=None):
    """非空客户端导出与同次实际GPU Cook都通过才交付；任一证据缺失时失败关闭。"""
    with csv_path.open(encoding='utf-8-sig', newline='') as stream:
        rows = [{key.strip(): str(value).strip() for key, value in row.items()} for row in csv.DictReader(stream)]
    if not rows or not {'Filename', 'ChunkType', 'Platform', 'Size'}.issubset(rows[0]):
        raise ValueError('不是非空引擎IoStore CSV')
    exports = {row['Filename'].replace('\\', '/').lower() for row in rows
               if row['ChunkType'] == 'ExportBundleData' and row['Platform'] == 'WindowsClient' and int(row['Size']) > 0}
    checked = validate_exports(exports)
    shaders = validate_cook_shaders(csv_path, cook_log_path)
    return dict(checked, Passed=not checked['Missing'] and not checked['Forbidden'] and shaders['Passed'],
                ContainerCsv=str(csv_path.resolve()), CookShaders=shaders,
                NetworkAccepted=False, ManualVisualAccepted=False)


def main():
    """报告仅写调用者指定证据；不删除/修改资源、容器、地图或配置。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', type=Path, required=True)
    parser.add_argument('--cook-log', type=Path, help='同次UATLogs下唯一Cook日志；省略时从CSV所属Cook目录查找')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = validate(args.csv, args.cook_log)
    payload = json.dumps(result, ensure_ascii=False, indent=2)
    if args.report:
        args.report.write_text(payload, encoding='utf-8')
    print(payload)
    return 0 if result['Passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
