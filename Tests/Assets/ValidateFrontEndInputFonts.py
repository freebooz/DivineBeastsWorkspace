# -*- coding: utf-8 -*-
"""通过正式编辑器/Monolith运行，验证真实输入样式而非声明文本。

第三层DBAUIPack_Core拥有控件资产，平台配置负责固定视口缩放。
本测试只读取资产，不创建控件、不修改字体、不读取输入内容或提交认证。
失败表示图源声明与实际Slate字体不一致；运行包及窗口尺寸须另行验收。
"""
import json
import unreal

checks = []
for screen, names in (
    ("Login", ("AccountInput", "PasswordInput")),
    ("CharacterCreate", ("CharacterNameInput",)),
):
    path = "/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_" + screen
    asset = unreal.load_asset(path)
    assert asset is not None, "缺少真实页面资产：" + path
    for name in names:
        widget = unreal.find_object(None, asset.get_path_name() + ":WidgetTree." + name)
        assert widget is not None, "缺少真实输入控件：" + name
        # UE5.8的有效字体在TextStyle.Font；旧WidgetStyle.Font声明不会改变渲染字体。
        font_size = widget.get_editor_property("widget_style").text_style.font.size
        scale = widget.get_editor_property("render_transform").scale
        checks.append({"screen": screen, "widget": name, "font_size": font_size,
                       "scale_x": scale.x, "scale_y": scale.y})
print(json.dumps(checks))
assert all(x["font_size"] == 16.0 and x["scale_x"] == 1.0 and x["scale_y"] == 1.0
           for x in checks), "输入框字体必须为固定16号，控件RenderScale必须为1"
