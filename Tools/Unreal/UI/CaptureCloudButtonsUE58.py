"""原生WidgetComponent浮点帧缓冲截图：UE执行一次sRGB编码，Python仅封装PNG。"""
from pathlib import Path
import json, struct, zlib, hashlib
import unreal as u

def capture():
    root=Path(__file__).resolve().parents[3]
    out=root/"Saved/Validation/GenericCloudButtons20261103"
    component=u._cloud_capture["pie_component"]
    target=component.get_render_target()
    assert target and target.get_editor_property("render_target_format")==u.TextureRenderTargetFormat.RTF_RGBA16F
    assert not component.get_editor_property("apply_gamma_correction")
    # 此UE接口对浮点RHI调用ToFColor(true)。后续不再做色彩转换、调色或缩放。
    pixels=u.RenderingLibrary.read_render_target(component,target,normalize=False)
    width=int(target.get_editor_property("size_x"));height=int(target.get_editor_property("size_y"))
    assert pixels is not None and len(pixels)==width*height
    rgba=bytearray()
    for p in pixels:rgba.extend((p.r,p.g,p.b,p.a))
    scanlines=bytearray()
    for y in range(height):
        scanlines.append(0);scanlines.extend(rgba[y*width*4:(y+1)*width*4])
    def chunk(tag,data):
        return struct.pack(">I",len(data))+tag+data+struct.pack(">I",zlib.crc32(tag+data)&0xffffffff)
    png=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,6,0,0,0))+chunk(b"sRGB",b"\0")+chunk(b"IDAT",zlib.compress(scanlines,6))+chunk(b"IEND",b"")
    name="NativeStates_213x60_sRGB.png"
    (out/name).write_bytes(png)
    raw=u.RenderingLibrary.read_render_target_raw_pixel(component,target,186,140,normalize=False)
    sampled=list(rgba[(140*width+186)*4:(140*width+186)*4+4])
    def srgb(value):return round(255*(12.92*value if value<=.0031308 else 1.055*max(value,0)**(1/2.4)-.055))
    expected=[srgb(raw.r),srgb(raw.g),srgb(raw.b)]
    assert max(abs(x-y) for x,y in zip(expected,sampled[:3]))<=1
    widget=component.get_user_widget_object()
    widgets={o.get_name():o for o in u.ObjectIterator(u.Widget) if o.get_path_name().startswith(widget.get_path_name()+".") or o.get_path_name().startswith(widget.get_path_name()+":")}
    disabled={name:widgets[name].get_is_enabled() for name in ("State_Primary_Disabled","State_Secondary_Disabled")}
    assert not any(disabled.values())
    report={"Image":name,"Size":[width,height],"DefaultButtonSize":[213,60],"NativePixelAPI":"UKismetRenderingLibrary::ReadRenderTarget / PF_FloatRGBA / ToFColor(true)","PNGEncodingOnly":True,"ImageCorrections":False,"RHI_RGBA_SHA256":hashlib.sha256(rgba).hexdigest(),"PNG_SHA256":hashlib.sha256(png).hexdigest(),"PixelLinear":[raw.r,raw.g,raw.b,raw.a],"PixelSRGB":sampled,"SingleSRGBEncodingPassed":True,"NativeDisabled":disabled,"RuntimePointerClickVerified":False,"HumanVisualAccepted":False}
    (out/"NativeCapture.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print("CLOUD_BUTTON_NATIVE_SRGB_CAPTURE",name,sampled)

capture()
