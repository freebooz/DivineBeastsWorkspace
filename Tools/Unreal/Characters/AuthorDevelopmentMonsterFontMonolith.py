#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Monolith创建开发石灵的中文距离场字体。来源为锁定UE随附DroidSansFallback，不分发Windows系统字体。
字体归DBAUIPack_Core；Common石灵仅保留中立TextRender槽，项目客户端经Data组合，避免内容包硬引用环。
Windows GDI字体只注册到本次编辑器进程并在finally撤销；不安装系统字体、不修改地图或已有字体。
"""
import argparse
import importlib.util
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FONT = '/DBAUIPack_Core/UI/Fonts/Development/F_DBA_DevelopmentMonsterLabel'
TEXT = '远古石灵 · 训练敌人生命 300 / 300已击败 · 等待重生0123456789'


def font_contract(path):
    """只读SFNT name/cmap4表，独立确认字体家族和所需BMP中文字形；不依赖额外字体软件包。"""
    data = path.read_bytes()
    u16 = lambda offset: struct.unpack_from('>H', data, offset)[0]
    tables = {}
    for index in range(u16(4)):
        tag, _, offset, size = struct.unpack_from('>4sIII', data, 12 + 16 * index)
        if offset + size > len(data):
            raise ValueError('字体表越界')
        tables[tag] = offset
    base = tables[b'name']
    names = []
    for index in range(u16(base + 2)):
        platform, encoding, language, name_id, size, offset = struct.unpack_from('>6H', data, base + 6 + 12 * index)
        if platform == 3 and name_id == 1:
            names.append(data[base + u16(base + 4) + offset:base + u16(base + 4) + offset + size].decode('utf-16-be'))
    if not names or len(set(names)) != 1:
        raise ValueError('缺少唯一Windows字体家族')
    base = tables[b'cmap']
    subtables = []
    for index in range(u16(base + 2)):
        platform, encoding, offset = struct.unpack_from('>HHI', data, base + 4 + 8 * index)
        if platform == 3 and encoding == 1 and u16(base + offset) == 4:
            subtables.append(base + offset)
    if len(subtables) != 1:
        raise ValueError('缺少唯一BMP Unicode字形表')
    base = subtables[0]
    count = u16(base + 6) // 2
    end = base + 14
    start, delta, ranges = end + 2 * count + 2, end + 4 * count + 2, end + 6 * count + 2
    missing = []
    for code in sorted(set(map(ord, TEXT)) - {32}):
        glyph = 0
        for index in range(count):
            if u16(start + 2 * index) <= code <= u16(end + 2 * index):
                offset = u16(ranges + 2 * index)
                glyph = u16(ranges + 2 * index + offset + 2 * (code - u16(start + 2 * index))) if offset else code
                if glyph:
                    glyph = (glyph + u16(delta + 2 * index)) & 65535
                break
        if not glyph:
            missing.append(chr(code))
    if missing:
        raise ValueError('源字体缺字：' + ''.join(missing))
    return names[0]


def run(engine_root, apply):
    """创建只允许新身份；校验使用保存资源实际字形尺寸，缺字明确失败，不回退系统字体。"""
    spec = importlib.util.spec_from_file_location('casting_transport', ROOT / 'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    transport = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(transport)
    protocol = transport.transport()
    source = engine_root / 'Engine/Content/Slate/Fonts/DroidSansFallback.ttf'
    family = font_contract(source)
    if apply:
        protocol.editor_python(f'''import ctypes, unreal
path={FONT!r}
if unreal.EditorAssetLibrary.does_asset_exist(path): raise RuntimeError('字体已存在，不覆盖')
gdi=ctypes.WinDLL('gdi32',use_last_error=True)
gdi.AddFontResourceExW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_void_p]
gdi.RemoveFontResourceExW.argtypes=gdi.AddFontResourceExW.argtypes
if not gdi.AddFontResourceExW({str(source)!r},0x10,None): raise RuntimeError('本编辑器私有字体注册失败')
try:
 factory=unreal.TrueTypeFontFactory()
 options=factory.get_editor_property('import_options')
 settings=options.get_editor_property('data')
 for key,value in {{'font_name':{family!r},'chars':{TEXT!r},'height':32.0,'include_ascii_range':True,'use_distance_field_alpha':True,'distance_field_scale_factor':4,'texture_page_width':512,'texture_page_max_height':512,'x_padding':4,'y_padding':4}}.items(): settings.set_editor_property(key,value)
 options.set_editor_property('data',settings)
 # create_asset不调用有交互字体对话框的ConfigureProperties；使用完整显式ImportOptions。
 asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('F_DBA_DevelopmentMonsterLabel','/DBAUIPack_Core/UI/Fonts/Development',unreal.Font,factory)
 if not asset or not unreal.EditorAssetLibrary.save_loaded_asset(asset,False): raise RuntimeError('字体生成/保存失败')
 print('MONSTER_FONT_CREATED',asset.get_path_name())
finally:
 if not gdi.RemoveFontResourceExW({str(source)!r},0x10,None): raise RuntimeError('私有字体撤销失败')''')
    result = protocol.editor_python(f'''import json, unreal
f=unreal.load_asset({FONT!r})
if not isinstance(f,unreal.Font): raise RuntimeError('缺少真实中文字体')
chars=f.get_editor_property('characters')
# 锁定UE TTFontImport.cpp：ASCII0..255先直接映射，其他Chars按Unicode升序建立CharRemap。
expected=sorted(set(range(256))|set(map(ord,{TEXT!r})))
if len(chars)!=len(expected): raise RuntimeError('真实字形数量与导入合同不一致')
checks=[]
for code in sorted(set(map(ord,{TEXT!r}))-set(range(256))):
 cell=chars[expected.index(code)]
 width,height=cell.get_editor_property('u_size'),cell.get_editor_property('v_size')
 if width<=0 or height<=0: raise RuntimeError('空中文字形：'+chr(code))
 checks.append({{'Character':chr(code),'Width':width,'Height':height}})
print('FONT_GLYPHS='+json.dumps({{'Family':f.get_editor_property('import_options').get_editor_property('font_name'),'Glyphs':checks,'CharacterCount':len(chars)}},ensure_ascii=False))''')
    (protocol.EVIDENCE / 'MonsterFontGlyphs.json').write_text(json.dumps({'Source': str(source), 'Family': family, 'Result': result, 'ManualVisualAccepted': False}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'Font': FONT, 'Evidence': str(protocol.EVIDENCE)}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine-root', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    run(args.engine_root, args.apply)
