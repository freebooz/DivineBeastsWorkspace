#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成UI主题的Monolith参数文件，不创建或修改任何UE二进制资源。

所属：神兽联盟第三层项目的编辑器辅助工具。输入为本文件明确的样式方案，输出到
Game/Saved/Monolith/UIThemeSpecs（可重建参数目录）；真正的蓝图/主题创建、属性写入、
编译及保存必须由Monolith逐项执行。生成参数成功不等于资产或自动化测试通过。
默认主题用深青灰与青铜，测试主题用中性灰蓝；两者共享语义键，不共享可变CDO。
字体仅引用合法引擎字体资产，不导出、复制或分发字体文件。颜色值为线性RGBA。
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
FONT = '/Engine/EngineFonts/Roboto.Roboto'
WHITE = '/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture'
GRAY = '/Engine/EngineResources/Gray.Gray'


def color(r: float, g: float, b: float, a: float = 1.0) -> dict[str, float]:
    """构造有限的线性RGBA颜色，禁止不合法值进入样式源参数。"""
    channels = (r, g, b, a)
    if not all(0.0 <= x <= 1.0 for x in channels):
        raise ValueError('颜色分量必须处于0到1之间')
    return dict(zip(('R', 'G', 'B', 'A'), channels))


def brush(fill: dict, edge: dict, *, resource: str = WHITE) -> dict[str, Any]:
    """原生Slate圆角画刷；资源、填色和描边由样式CDO拥有，控件只复制已加载数据。"""
    return {
        'DrawAs': 'RoundedBox',
        'ImageSize': {'X': 64.0, 'Y': 40.0},
        'ResourceObject': resource,
        'TintColor': {'SpecifiedColor': fill, 'ColorUseRule': 'UseColor_Specified'},
        'OutlineSettings': {
            'CornerRadii': {'X': 6.0, 'Y': 6.0, 'Z': 6.0, 'W': 6.0},
            'Color': {'SpecifiedColor': edge, 'ColorUseRule': 'UseColor_Specified'},
            'Width': 1.0, 'RoundingType': 'FixedRadius',
            'bUseBrushTransparency': True,
        },
    }


def button_style(base: dict, hover: dict, pressed: dict, edge: dict,
                 text_class: str) -> dict[str, Any]:
    """提供六种选中/按压状态和禁用状态；不在页面中另外维护同一套状态颜色。"""
    normal = brush(base, edge)
    hovered = brush(hover, edge)
    pressed_brush = brush(pressed, edge)
    return {
        'bSingleMaterial': False,
        'NormalBase': normal, 'NormalHovered': hovered, 'NormalPressed': pressed_brush,
        'SelectedBase': hovered, 'SelectedHovered': hovered, 'SelectedPressed': pressed_brush,
        'Disabled': brush(color(.025, .03, .03), color(.12, .12, .12)),
        'NormalTextStyle': text_class, 'NormalHoveredTextStyle': text_class,
        'SelectedTextStyle': text_class, 'SelectedHoveredTextStyle': text_class,
        'DisabledTextStyle': text_class,
    }


def build_specs(variant: str) -> list[dict[str, Any]]:
    """构建同一套语义的两种项目外观；测试皮肤只归Development目录。"""
    is_default = variant == 'Default'
    if variant not in ('Default', 'Neutral'):
        raise ValueError('未知主题变体')
    folder = '/DBAUIPack_Core/UI/Styles' if is_default else '/Game/Development/UITheme/Styles'
    prefix = 'BP_DBA' if is_default else 'BP_Test'
    names = {key: folder + '/' + prefix + '_' + key for key in (
        'TextStyle_Body', 'TextStyle_Title', 'ButtonStyle_Primary',
        'ButtonStyle_Secondary', 'BorderStyle_Panel')}
    def generated_class(path: str) -> str:
        return path + '.' + path.rsplit('/', 1)[-1] + '_C'
    edge = color(.43, .28, .105) if is_default else color(.15, .24, .36)
    body = color(.84, .82, .74) if is_default else color(.82, .88, .94)
    title = color(.75, .55, .25) if is_default else color(.38, .66, .92)
    primary = color(.035, .115, .085) if is_default else color(.035, .075, .14)
    hover = color(.06, .19, .13) if is_default else color(.06, .13, .24)
    pressed = color(.02, .055, .035) if is_default else color(.02, .04, .08)
    specs = []
    for key, tint, size, face in (
        ('TextStyle_Body', body, 16, 'Regular'),
        ('TextStyle_Title', title, 24, 'Bold'),
    ):
        specs.append({'asset_path': names[key], 'parent_class': 'CommonTextStyle',
                      'properties': {'Font': {'FontObject': FONT, 'TypefaceFontName': face,
                                               'Size': size}, 'Color': tint,
                                     'bUsesDropShadow': False}})
    for key, base, over, down in (
        ('ButtonStyle_Primary', primary, hover, pressed),
        ('ButtonStyle_Secondary', color(.035, .045, .045), color(.07, .08, .08), color(.02, .025, .025)),
    ):
        specs.append({'asset_path': names[key], 'parent_class': 'CommonButtonStyle',
                      'properties': button_style(base, over, down, edge,
                                                 generated_class(names['TextStyle_Body']))})
    specs.append({'asset_path': names['BorderStyle_Panel'], 'parent_class': 'CommonBorderStyle',
                  'properties': {'Background': brush(color(.012, .024, .021) if is_default
                                                     else color(.065, .08, .105), edge,
                                                     resource=WHITE if is_default else GRAY)}})
    rules = {'Buttons': [], 'Texts': [], 'Borders': []}
    required = []
    for group, key, style, kind in (
        ('Buttons', 'UI.Style.Button.Primary', 'ButtonStyle_Primary', 'Button'),
        ('Buttons', 'UI.Style.Button.Secondary', 'ButtonStyle_Secondary', 'Button'),
        ('Texts', 'UI.Style.Text.Body', 'TextStyle_Body', 'Text'),
        ('Texts', 'UI.Style.Text.Title', 'TextStyle_Title', 'Text'),
        ('Borders', 'UI.Style.Panel.Default', 'BorderStyle_Panel', 'Border'),
    ):
        rules[group].append({'Rule': {'StyleId': key, 'Scope': 'Project'},
                             'StyleClass': generated_class(names[style])})
        required.append({'Kind': kind, 'StyleId': key})
    theme_path = ('/DBAUIPack_Core/UI/Themes/DA_DBA_UITheme_Default' if is_default else
                  '/Game/Development/UITheme/DA_Test_UITheme_Neutral')
    specs.append({'asset_path': theme_path,
                  'class_name': '/Script/GamePlatformUIClient.GamePlatformUIThemeDefinition',
                  'properties': {'LogicalId': {'Namespace': 'dba.ui.theme' if is_default else 'test.ui.theme',
                                                'Name': 'default' if is_default else 'neutral', 'LogicalVersion': 1},
                                 'DataVersion': {'SchemaVersion': 1, 'ContentRevision': 1},
                                 **rules, 'RequiredStyles': required}})
    return specs


def main() -> int:
    """仅写入可重建JSON参数；不调用引擎、不提前运行自动化，也不写.uasset占位文件。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=('Default', 'Neutral', 'All'), default='All')
    args = parser.parse_args()
    out = ROOT / 'Game/Saved/Monolith/UIThemeSpecs'
    out.mkdir(parents=True, exist_ok=True)
    variants = ('Default', 'Neutral') if args.variant == 'All' else (args.variant,)
    for variant in variants:
        specs = build_specs(variant)
        for index, spec in enumerate(specs):
            path = out / (variant + '_' + str(index + 1).zfill(2) + '.json')
            path.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        print(variant + ': ' + str(len(specs)) + '个Monolith参数文件 -> ' + str(out))
    print('仅生成源参数；实际蓝图/主题与验收结果必须由Monolith另行生成和回读。')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
