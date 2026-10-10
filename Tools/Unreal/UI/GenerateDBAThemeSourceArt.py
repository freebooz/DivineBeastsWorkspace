#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》原创青铜云纹面板和玉色按钮四态源纹理。

仅生成 PNG 源图与可追溯清单；真实 UE Texture2D 及 CommonUI Brush 必须
由已确认工程的 Monolith MCP 创建并保存，不用 Python 伪造 .uasset。
"""
import hashlib
import json
import math
import random
from pathlib import Path
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Theme"
MOUNT = "/DBAUIPack_Core/UI/Textures/Theme"

def lerp(a,b,t):
    return tuple(round(x+(y-x)*t) for x,y in zip(a,b))

def gradient(size, upper, lower, opacity=255):
    w,h=size
    image=Image.new("RGBA",size,(0,0,0,0))
    draw=ImageDraw.Draw(image)
    for y in range(h):
        draw.line((0,y,w-1,y),fill=(*lerp(upper,lower,y/max(1,h-1)),opacity))
    return image

def panel_frame():
    s=1024
    image=gradient((s,s),(25,45,42),(12,27,26),250)
    alpha=Image.new("L",(s,s),0)
    ImageDraw.Draw(alpha).rounded_rectangle((16,16,s-17,s-17),radius=58,fill=255)
    image.putalpha(alpha)
    draw=ImageDraw.Draw(image,"RGBA")
    randomizer=random.Random(20261010)
    # 内衬细微磨砂和边缘水纹，使高对比交互区域保持安静。
    for _ in range(8000):
        x=randomizer.randrange(100,924); y=randomizer.randrange(100,924)
        draw.point((x,y),fill=(159,187,161,randomizer.randint(3,11)))
    for i in range(12):
        y=157+i*59
        line=[(x,round(y+5*math.sin(x/51+i))) for x in range(130,894,14)]
        draw.line(line,fill=(70,127,102,10),width=2)
    draw.rounded_rectangle((24,24,999,999),radius=56,outline=(126,94,53,211),width=7)
    draw.rounded_rectangle((37,37,986,986),radius=44,outline=(194,154,89,211),width=3)
    draw.rounded_rectangle((54,54,969,969),radius=32,outline=(77,110,94,151),width=2)
    for y in (42,982):
        draw.polygon([(512,y-14),(534,y),(512,y+14),(490,y)],outline=(209,170,105,218))
        draw.ellipse((508,y-4,516,y+4),fill=(100,151,119,194))
        draw.line((446,y,482,y),fill=(173,132,73,195),width=2)
        draw.line((542,y,578,y),fill=(173,132,73,195),width=2)
    # 单角手绘式云纹后镜像为四角，九宫格可保留原创角纹的独立完整性。
    cloud=Image.new("RGBA",(s,s),(0,0,0,0))
    c=ImageDraw.Draw(cloud,"RGBA")
    c.arc((75,73,182,181),165,351,fill=(211,169,95,197),width=4)
    c.arc((91,93,155,156),286,172,fill=(125,171,143,176),width=3)
    c.arc((107,110,149,151),161,371,fill=(214,184,121,203),width=3)
    c.line([(66,134),(82,134),(105,151),(126,151)],fill=(194,155,82,161),width=3)
    c.line([(135,65),(135,85),(153,109),(153,124)],fill=(194,155,82,161),width=3)
    for x,y in ((86,107),(108,86),(144,145)):
        c.ellipse((x-3,y-3,x+3,y+3),fill=(216,177,101,191))
    for layer in (cloud,ImageOps.mirror(cloud),ImageOps.flip(cloud),ImageOps.flip(ImageOps.mirror(cloud))):
        image.alpha_composite(layer)
    return image

STATES={
 "Primary":{
  "Normal":((25,65,57),(13,42,38),(176,138,75),232),
  "Hovered":((40,97,80),(16,69,59),(225,177,94),252),
  "Pressed":((12,47,40),(8,34,29),(185,147,81),236),
  "Disabled":((55,68,62),(34,46,43),(99,105,95),130),
 },
 "Secondary":{
  "Normal":((41,56,51),(23,38,35),(143,117,72),175),
  "Hovered":((54,77,68),(29,55,50),(198,155,84),224),
  "Pressed":((28,48,43),(18,35,31),(147,119,70),193),
  "Disabled":((56,61,57),(44,49,46),(86,95,83),117),
 },
}

def button_frame(role,state):
    w,h=640,160
    upper,lower,metal,opacity=STATES[role][state]
    img=gradient((w,h),upper,lower,244 if state!="Disabled" else 202)
    mask=Image.new("L",(w,h),0)
    ImageDraw.Draw(mask).rounded_rectangle((9,10,w-10,h-11),radius=29,fill=255)
    img.putalpha(mask)
    draw=ImageDraw.Draw(img,"RGBA")
    draw.rounded_rectangle((13,14,w-14,h-15),radius=26,outline=(*metal,opacity),width=5)
    draw.rounded_rectangle((24,24,w-25,h-25),radius=20,outline=(*metal,62),width=2)
    draw.line((58,36,w-58,36),fill=(145,200,169,72 if state=="Hovered" else 28),width=3)
    for x in (52,w-52):
        draw.polygon([(x-11,80),(x,69),(x+11,80),(x,91)],outline=(*metal,opacity//2))
    return img

def save(name,image):
    dest=OUT/(name+".png")
    image.save(dest,optimize=True)
    data=dest.read_bytes()
    return {"Source":dest.relative_to(ROOT).as_posix(),"Destination":MOUNT+"/"+name,
            "Size":[image.width,image.height],"Bytes":len(data),
            "SHA256":hashlib.sha256(data).hexdigest()}

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    files=[save("T_DBA_UI_PanelFrame_BronzeCloud",panel_frame())]
    for role,states in STATES.items():
        for state in states:
            files.append(save(f"T_DBA_UI_Button_{role}_{state}",button_frame(role,state)))
    record={"SchemaVersion":1,"Count":len(files),"ImportedIntoUE":False,
            "DescriptionZh":"原创东方神话青铜云纹面板及主要/次要按钮四态；文本、LOGO及交互不烘入纹理。",
            "PanelNineSliceMargin":[0.09,0.09,0.09,0.09],
            "ButtonNineSliceMargin":[0.10,0.23,0.10,0.23],"Files":files}
    (OUT/"UIThemeSourceArtManifest.json").write_text(
        json.dumps(record,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("THEME_PNG_FILES",len(files))
    for item in files:print(item["Source"],item["Bytes"],item["Size"])
    print("IMPORT_TO_UE=NOT_YET_DONE")

if __name__=="__main__": main()
