#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""自然环境纹理人工审核预览（离线源RGBA合成），不替代UE材质GPU渲染或人眼签收。"""
import hashlib
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np

ROOT=Path(__file__).resolve().parents[4]
SRC=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
CATALOG=json.loads((SRC/"AmbientNatureStyleCatalog.json").read_text(encoding="utf-8"))["styles"]
DEST=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary"
DEST.mkdir(parents=True,exist_ok=True)
styles=[s for s in CATALOG if s["assetStatus"]=="planned"]
columns=6
cw,ch=326,273
sheet=Image.new("RGB",(columns*cw,math.ceil(len(styles)/columns)*ch),(27,31,38))
draw=ImageDraw.Draw(sheet)
fontpath="C:/Windows/Fonts/msyh.ttc"
font=ImageFont.truetype(fontpath,17) if Path(fontpath).exists() else ImageFont.load_default()
stats=[]
for index,style in enumerate(styles):
    x0=(index%columns)*cw
    y0=(index//columns)*ch
    path=SRC/"Textures"/f'T_DBA_Ambient_{style["styleId"]}_Albedo_Source.png'
    image=Image.open(path).convert("RGBA")
    a=np.asarray(image.getchannel("A"),dtype=np.uint8)
    coverage=float(np.mean(a>=128))
    partial=float(np.mean((a>0)&(a<255)))
    rgb=np.asarray(image,dtype=np.uint8)[:,:,:3]
    visible=np.repeat(a[:,:,None]>=128,3,axis=2)
    medium=np.mean(rgb[visible].reshape(-1,3),axis=0) if np.any(visible) else np.zeros(3)
    if image.size!=(1024,1024):
        raise RuntimeError("非预期1024像素原图："+str(path))
    preview=image.resize((150,150),Image.Resampling.LANCZOS)
    for j,bg in enumerate(((40,45,52),(225,225,220))):
        square=Image.new("RGBA",(152,152),bg+(255,))
        square.alpha_composite(preview,(1,1))
        sheet.paste(square.convert("RGB"),(x0+8+j*160,y0+9))
    label=f'{style["styleId"]}  {style["displayNameZh"]}'
    draw.text((x0+10,y0+169),label,font=font,fill=(222,230,230))
    draw.text((x0+10,y0+198),f"alpha coverage:{coverage*100:4.1f}%   edges:{partial*100:4.1f}%",font=font,fill=(190,210,212))
    draw.text((x0+10,y0+229),f"meanRGB:{int(medium[0])},{int(medium[1])},{int(medium[2])}",font=font,fill=(190,210,212))
    stats.append({"id":style["styleId"],"category":style["category"],"nameZh":style["displayNameZh"],
        "coveragePct":round(coverage*100,2),"partialAlphaPct":round(partial*100,2),
        "meanVisibleRGB":[round(float(x),1) for x in medium],
        "fileHash":hashlib.sha256(path.read_bytes()).hexdigest()})
sheet.save(DEST/"AmbientArtHumanReview.png",optimize=True)
(DEST/"AmbientArtHumanReview.json").write_text(json.dumps({"reviewType":"sourceAlphaOfflineComposite","notUnrealMaterialRender":True,"items":stats},ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
print("AMBIENT_SOURCE_VISUAL_REVIEW_READY",len(styles),"image",str(DEST/"AmbientArtHumanReview.png"),"avgCover",round(sum(x["coveragePct"] for x in stats)/len(stats),1))
