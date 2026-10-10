#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""经Monolith MCP/正式UE5.8 Editor把主题纹理写入CommonUI蓝图样式CDO。

不修改项目其他Widget、不创建第二套UI服务、不修改共享CommonUI原生类CDO。
所有改动都落在DBAUIPack_Core的现有三个项目样式Blueprint中。
"""
from __future__ import annotations

import unreal

MOUNT = "/DBAUIPack_Core/UI/Textures/Theme"
STYLES = "/DBAUIPack_Core/UI/Styles"
DEFAULT_MARGIN_PANEL = (0.09,0.09,0.09,0.09)
DEFAULT_MARGIN_BUTTON = (0.10,0.23,0.10,0.23)
BUTTON_FIELDS = {
    "NormalBase":"Normal",
    "NormalHovered":"Hovered",
    "NormalPressed":"Pressed",
    "Disabled":"Disabled",
    "SelectedBase":"Hovered",
    "SelectedHovered":"Hovered",
    "SelectedPressed":"Pressed",
}

def texture(name):
    path = MOUNT + "/" + name
    asset=unreal.EditorAssetLibrary.load_asset(path)
    if asset is None or asset.get_class().get_name()!="Texture2D":
        raise RuntimeError(f"项目主题真实Texture2D不存在：{path}")
    return asset

def brush_for(cdo,field,tex,margin,width,height):
    brush=cdo.get_editor_property(field)
    brush.set_editor_property("resource_object",tex)
    brush.set_editor_property("draw_as",unreal.SlateBrushDrawType.BOX)
    brush.set_editor_property("image_type",unreal.SlateBrushImageType.FULL_COLOR)
    brush.set_editor_property("margin",unreal.Margin(left=margin[0],top=margin[1],right=margin[2],bottom=margin[3]))
    size=unreal.DeprecateSlateVector2D()
    size.set_editor_property("x",float(width))
    size.set_editor_property("y",float(height))
    brush.set_editor_property("image_size",size)
    # 去除原RoundedBox的青黑乘色，PNG现在是背景颜色唯一真源；不修改字体/阴影/布局和事件。
    color=brush.get_editor_property("tint_color")
    color.set_editor_property("specified_color",unreal.LinearColor(1,1,1,1))
    brush.set_editor_property("tint_color",color)
    cdo.set_editor_property(field,brush)
    value=cdo.get_editor_property(field)
    actual=value.get_editor_property("resource_object")
    if actual!=tex:
        raise RuntimeError(f"{field}导入后资源未保持一致")
    print("BRUSH_BOUND",field,tex.get_name(),value.get_editor_property("draw_as"),
          float(value.get_editor_property("image_size").get_editor_property("x")),
          float(value.get_editor_property("image_size").get_editor_property("y")))

def style_cdo(style_name,expected_native):
    path=STYLES+"/"+style_name
    cls=unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls is None:
        raise RuntimeError("找不到现有项目UI样式蓝图："+path)
    cdo=unreal.get_default_object(cls)
    if cdo is None or not isinstance(cdo, getattr(unreal,expected_native)):
        raise RuntimeError("项目样式基类不匹配："+path)
    return path,cdo

def save_style(path):
    saved=unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False)
    if not saved:
        raise RuntimeError("Monolith源UI资源保存失败："+path)
    print("STYLE_ASSET_SAVED",path)

def run():
    panel=texture("T_DBA_UI_PanelFrame_BronzeCloud")
    textures={}
    for family in ["Primary","Secondary"]:
        textures[family]={state:texture(f"T_DBA_UI_Button_{family}_{state}")
                          for state in ["Normal","Hovered","Pressed","Disabled"]}
    path,cdo=style_cdo("BP_DBA_BorderStyle_Panel","CommonBorderStyle")
    brush_for(cdo,"Background",panel,DEFAULT_MARGIN_PANEL,1024,1024)
    save_style(path)
    for family in ["Primary","Secondary"]:
        path,cdo=style_cdo("BP_DBA_ButtonStyle_"+family,"CommonButtonStyle")
        for field,state in BUTTON_FIELDS.items():
            brush_for(cdo,field,textures[family][state],DEFAULT_MARGIN_BUTTON,640,160)
        save_style(path)
    print("COMMONUI_BLUEPRINT_STYLE_CLASSES_UPDATED",3)
    print("ORIGINAL_TEXTURES_LINKED",9)
    print("AUTOMATION_TESTS_NOT_RUN=TRUE")

run()
