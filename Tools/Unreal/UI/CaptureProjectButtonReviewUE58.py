"""读取真实运行的镜头工具按钮与浮点RHI；只封装引擎像素，不修图、不合成效果。"""
from pathlib import Path
import json, struct, zlib, hashlib
import unreal as u

def capture_camera():
    """验证未初始化主题的真实新实例，并导出原生帧；临时PIE由调用方负责结束。"""
    out=Path(__file__).resolve().parents[3]/'Saved/Validation/AllButtons20261104'
    state=u._ab_preview
    widget=state['pie_widget'];component=state['pie_component'];buttons=state['buttons']
    assert len(buttons)==9
    player=widget.get_owning_local_player()
    manager=next((x for x in u.ObjectIterator(u.GamePlatformUIManagerSubsystem) if x.get_outer()==player),None)
    assert manager,'未找到实际LocalPlayer主题服务'
    theme=manager.get_current_theme_id()
    assert str(theme.get_editor_property('primary_asset_name')) in ('None',''),'本测试禁止预设主题掩盖初始样式缺失'
    rows=[]
    for button in buttons:
        style=button.get_editor_property('widget_style');states={}
        for field,phase in [('normal','Normal'),('hovered','Hovered'),('pressed','Pressed'),('disabled','Disabled')]:
            resource=style.get_editor_property(field).get_editor_property('resource_object')
            path=resource.get_path_name().split('.')[0] if resource else None
            expected='/DBAUIPack_Core/UI/Materials/Buttons/MI_DBA_UI_Button_Secondary_'+phase
            assert path==expected,(button.get_name(),field,path)
            states[field]=path
        rows.append({'Button':button.get_name(),'States':states})
    target=component.get_render_target()
    assert target and target.get_editor_property('render_target_format')==u.TextureRenderTargetFormat.RTF_RGBA16F
    assert not component.get_editor_property('apply_gamma_correction')
    width=target.get_editor_property('size_x');height=target.get_editor_property('size_y')
    pixels=u.RenderingLibrary.read_render_target(component,target,normalize=False)
    assert len(pixels)==width*height
    rgba=bytearray()
    for pixel in pixels:rgba.extend((pixel.r,pixel.g,pixel.b,pixel.a))
    scanlines=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'sRGB',b'\0')+chunk(b'IDAT',zlib.compress(scanlines,6))+chunk(b'IEND',b'')
    (out/'DynamicCamera_Native.png').write_bytes(png)
    report={'Passed':True,'NoCurrentTheme':True,'CurrentTheme':str(theme),'Buttons':rows,
            'RuntimeWidget':widget.get_path_name(),'World':widget.get_world().get_path_name(),
            'NativeParentVerified':isinstance(widget,u.GamePlatformWidgetBase),
            'NativeRGBA16F':True,'SingleEngineSRGBEncoding':True,'PNG_SHA256':hashlib.sha256(png).hexdigest(),
            'HumanInputAccepted':False}
    (out/'DynamicCamera.Native.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('NATIVE_CAMERA_NO_THEME_PASS',len(rows),width,height)
