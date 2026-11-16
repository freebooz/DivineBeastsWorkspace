# -*- coding: utf-8 -*-
"""《神兽联盟》冰霜双掌聚能 V1.0：生成原创柔雾/旋流/冷核/冰晶 RGBA 纹理与连续16帧图集。
不调用外部下载或绘图服务，所有输出是RGBA PNG源图而不是.uasset。
连续序列由相同解析函数按相位平滑推进，不拼接互不相关的概念图。
"""
import math
import zlib
import struct
import hashlib
import json
import pathlib
from datetime import datetime, timezone
ROOT=pathlib.Path(__file__).resolve().parents[4]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/PalmElementGather/Review/V1'
PI=math.pi
def clamp(v,a=0,b=1):return max(a,min(b,v))
def gauss(x,s):return math.exp(-(x/max(s,.0001))**2)
def fbm(x,y,t):return .52*math.sin(5.13*x+2.65*y+.4*math.sin(3.1*y+t)+t)+.25*math.sin(9.7*y-6.1*x+.3*math.sin(5.1*x-t)-t*.8)+.13*math.sin(14.7*x+13.3*y+t*.73)
def pixel(kind,x,y,t):
    r=math.hypot(x,y)
    theta=math.atan2(y,x)
    edge=math.exp(-min(200,abs(r/.94)**6))
    noise=fbm(x,y,t)
    if kind=='mist':
        warped=r+.055*noise+.06*math.sin(theta*4+r*8.5+t)
        wispa=gauss(warped-(.45+.13*math.cos(theta*2-t)),.22)
        wispb=gauss(warped-(.25+.12*math.sin(theta*3+t)),.26)
        density=(.48*wispa+.31*wispb+.25*gauss(r,.58))*(.83+.16*math.sin(6*theta+9*r-t+.5*noise))
        alpha=clamp(.73*density*edge,0,.62)
        g=gauss(r,.28)
        color=(int(clamp((174+31*g)/255)*255),int(clamp((213+23*g)/255)*255),255)
    elif kind=='flow':
        # 动态双层环状涡旋：随相位连续卷入，中央用微量密度避免空洞
        ring=.45+.16*math.sin(theta*2+t)
        delta=r-ring-.025*noise
        spiral=gauss(delta,.092)*(.40+.6*gauss(math.sin((theta+5.5*r-t)*.85),.87))
        inner=gauss(r-(.28+.12*math.sin(3*theta-t-.8)),.066)*.34
        alpha=clamp((.68*spiral+.28*inner+.10*gauss(r,.23))*edge,0,.85)
        highlight=gauss(delta,.1)
        color=(int(168+76*highlight),int(216+36*highlight),255)
    elif kind=='core':
        g=gauss(r,.28); halo=gauss(r,.61)
        vein=.26*gauss(math.sin(theta*6+r*18)*r,.14)*gauss(r,.7)
        alpha=clamp(.81*g+.17*halo+.28*vein*(.5+.5*math.sin(theta*7+r*22+t*.8)),0,.93)*edge
        color=(int(164+88*g),int(205+49*g),255)
    elif kind=='crystal':
        # 单个四棱晶体：依照中心位置/宽度计算柔和透明面，偏冰蓝而非火焰染色
        yp=y+.12*math.sin(t)
        w=.38*max(0,1-abs(yp)/.8)
        boundary=abs(x)-w
        inside=clamp(-boundary/.045)
        cap=clamp((.8-abs(yp))/.06)
        body=inside*cap
        facet=clamp(.65+.35*math.cos((x/w if w else 0)*PI*.5))
        ridge=gauss(x-.10*math.sin(yp*4+t),.045)*inside
        alpha=clamp(body*(.40+.25*facet)+.28*ridge,0,.83)
        color=(int(166+72*ridge),int(210+37*facet),255)
        alpha*=edge
    else:raise ValueError(kind)
    return (*color,int(255*alpha))
def chunk(name,data):return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
def render(path,kind,side,frames):
    if path.exists():raise RuntimeError('审核源纹理已经存在，不覆盖：'+str(path))
    if frames not in (1,16):raise ValueError(frames)
    tile=side if frames==1 else side//4
    data=bytearray()
    for y in range(side):
        data.append(0)
        fy=y//tile; iy=y%tile
        yy=(iy+.5)/tile*2-1
        for x in range(side):
            fx=x//tile;ix=x%tile
            xx=(ix+.5)/tile*2-1
            frame=fy*4+fx if frames==16 else 0
            t=2*PI*frame/frames
            data.extend(pixel(kind,xx,yy,t))
    ihdr=struct.pack('>IIBBBBB',side,side,8,6,0,0,0)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',ihdr)+chunk(b'IDAT',zlib.compress(data,6))+chunk(b'IEND',b'')
    path.write_bytes(png)
    del data
    return {'file':path.name,'width':side,'height':side,'frames':frames,'rgba':True,
            'bytes':len(png),'sha256':hashlib.sha256(png).hexdigest(),'kind':kind}
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    manifest={'source':str(OUT),'createdUTC':datetime.now(timezone.utc).isoformat(),'assets':[],
             'manualApproved':False,'actualNiagaraBound':False,'continuity':'analytic phase 0..2pi across 16 tiles'}
    for name,kind,size,frames in [
        ('T_DBA_PalmIce_MistSoft_Review.png','mist',512,1),
        ('T_DBA_PalmIce_SpiralFlow_Review.png','flow',512,1),
        ('T_DBA_PalmIce_Core_Review.png','core',512,1),
        ('T_DBA_PalmIce_CrystalSeed_Review.png','crystal',512,1),
        ('T_DBA_PalmIce_MistFlipbook_4x4_Review.png','mist',1024,16),
        ('T_DBA_PalmIce_SpiralFlipbook_4x4_Review.png','flow',1024,16),
    ]:
        a=render(OUT/name,kind,size,frames)
        manifest['assets'].append(a)
        print('PALM_ICE_TEXTURE_CREATED',name,a['bytes'],flush=True)
    (OUT/'TextureManifest_V1_Review.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('PALM_ICE_TEXTURES_DONE',len(manifest['assets']),str(OUT),flush=True)
if __name__=='__main__':main()
