#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM冰箭源资产门禁：验证真实PNG的透明边界/遮罩、法线与网格源文件。
本文件属于跨系统测试，不加载引擎、不创建源图、不改变玩法；成功仅证明源资产合同，
不能替代UE原生资产编译、视觉验收、联机与Cook。失败返回1并逐项打印缺口。
"""
from pathlib import Path
import importlib.util
import sys
import tempfile
import types
from unittest.mock import patch
import numpy as np
from PIL import Image
ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow'
SPRITES = ('RuneOuter', 'RuneInner', 'CrystalShard', 'FrostMist', 'TrailStrip', 'IceFlake', 'ImpactFlash', 'FrozenGround')

def inspect_sources():
    """读取确定性的独立技术纹理，拒绝背景实底、裁边及缺失文件；不接触参考PNG。"""
    failures = []
    for name in SPRITES:
        p = SOURCE / ('T_DBA_IceArrow_' + name + '.png')
        if not p.is_file():
            failures.append('缺少独立透明纹理：' + name)
            continue
        with Image.open(p) as im:
            if im.mode != 'RGBA' or im.width < 512 or im.height < 512:
                failures.append('尺寸/透明格式不合格：' + name)
                continue
            a = np.asarray(im)[..., 3]
            border = np.concatenate((a[0], a[-1], a[:,0], a[:,-1]))
            if border.max() != 0 or a.max() < 100 or np.mean(a == 0) < .03:
                failures.append('透明通道或留边不合格：' + name)
    for suffix in ('CrackMask', 'IceNormal', 'IceORM'):
        p = SOURCE / ('T_DBA_IceArrow_' + suffix + '.png')
        if not p.is_file():
            failures.append('缺少技术贴图：' + suffix)
        elif suffix == 'IceNormal':
            v = np.asarray(Image.open(p).convert('RGB')).astype(float) / 127.5 - 1
            if np.mean(abs(np.linalg.norm(v, axis=2)-1)) > .04 or v[...,2].min() < .3:
                failures.append('切线空间法线未归一化或朝向异常')
    for name in ('CrystalLance', 'CrystalShard', 'IceCrown', 'TrailRibbon'):
        p = SOURCE / ('SM_DBA_IceArrow_' + name + '.obj')
        if not p.is_file():
            failures.append('缺少网格源：' + name)
        else:
            text = p.read_text('utf-8')
            if text.count('\nv ') < 6 or text.count('\nf ') < 8 or '\nvt ' not in text or '\nvn ' not in text:
                failures.append('网格三角面/UV/法线不足：' + name)
    return failures


def inspect_sequence_isolation():
    """用纯Python检查每轮截图独占目录；不启动UE、不执行Niagara，也不触及正式资产。"""
    problems = []
    script = ROOT / 'Tools/Unreal/VFX/DevelopmentMagic/ReviewIceArrowSequenceA.py'
    if not script.is_file():
        return ['缺少寒冰箭逐帧审核脚本']
    # Unreal模块只在定义真实审核类时被引用；隔离目录函数可在普通Python中独立验证。
    with patch.dict(sys.modules, {'unreal': types.ModuleType('unreal')}):
        spec = importlib.util.spec_from_file_location('ice_arrow_sequence_isolation_test', script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix='sslm_ice_arrow_review_') as temp:
        folder = Path(temp)
        previous = folder / 'SequenceA'
        previous.mkdir()
        original = previous / 'Frame_0030.png'
        original.write_bytes(b'previous-review-evidence')
        first_id, first = module.reserve_sequence_directory(folder / 'SequenceRuns')
        first_report = first / 'SequenceReport.json'
        first_report.write_text('{"round":1}', encoding='utf-8')
        second_id, second = module.reserve_sequence_directory(folder / 'SequenceRuns')
        if first_id == second_id or first == second or not first.is_dir() or not second.is_dir():
            problems.append('连续两次审查没有获得独立目录')
        if not first_report.is_file() or first_report.read_text(encoding='utf-8') != '{"round":1}':
            problems.append('第二轮审查覆盖第一轮报告')
        if original.read_bytes() != b'previous-review-evidence':
            problems.append('新版审查修改了历史SequenceA帧')
    return problems


if __name__ == '__main__':
    problems = inspect_sources() + inspect_sequence_isolation()
    for problem in problems: print('FAIL', problem)
    print('ICE_ARROW_SOURCE_GATE', 'FAIL' if problems else 'PASS', 'issues=', len(problems))
    sys.exit(1 if problems else 0)
