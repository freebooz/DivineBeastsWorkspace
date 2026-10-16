#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM冰箭技术源资产制作：透明遮罩/真实切线法线/ORM/原创晶体OBJ。
所属项目制作工具层，仅输出DBAPresentationPack_Core/SourceArt/VFX/IceArrow。
本脚本不生成UE二进制，不联网，不把设计展板充当纹理；--apply显式写入，
目录已存在则拒绝覆盖。输出由后续UE导入器消费，删除/替换须独立审核。
"""
from pathlib import Path
import argparse, hashlib, json, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
ROOT = Path(__file__).resolve().parents[4]
DEST = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow'
N = 1024

def smooth(v):
    """连续三次平滑曲线，避免渐隐贴图出现硬边；输入为归一化数值。"""
    t=np.clip(v,0,1); return t*t*(3-2*t)

def save_sprite(name, mask):
    """白色RGB加独立Alpha，发光颜色由材质控制；边界强制透明避免粒子方框。"""
    raw=np.asarray(mask)
    a=raw.astype(float)
    # Pillow字节遮罩需归一化；浮点层叠值可超过1，必须裁剪而非误按字节除255。
    if np.issubdtype(raw.dtype,np.integer): a=a/255.
    a=np.clip(a,0,1); a[[0,-1],:]=0; a[:,[0,-1]]=0
    rgba=np.full((*a.shape,4),255,dtype=np.uint8); rgba[...,3]=(a*255+.5).astype(np.uint8)
    Image.fromarray(rgba).save(DEST/('T_DBA_IceArrow_'+name+'.png'))

def frost_noise(size=1024):
    """固定种子的多尺度连续噪声，仅用于透明度破碎，不是性能或游戏模拟数据。"""
    rng=np.random.default_rng(351015); out=np.zeros((size,size),dtype=np.float32)
    for res,weight in ((6,.50),(12,.25),(24,.14),(48,.075),(96,.035)):
        im=Image.fromarray((rng.random((res,res))*255).astype('uint8')).resize((size,size),Image.Resampling.BICUBIC)
        out+=np.asarray(im,dtype=np.float32)/255*weight
    return out

def rune(inner=False):
    """原创环形符印：同心细线、断弧、晶叶与非文字几何符记；正交俯视且四周留白。"""
    n=2048; im=Image.new('L',(n,n),0); d=ImageDraw.Draw(im); c=n/2
    def point(r,a): return (c+math.cos(a)*r*n,c+math.sin(a)*r*n)
    def line(points,w=3,value=255): d.line(points,fill=value,width=w,joint='curve')
    if not inner:
        for r,w in ((.345,3),(.353,2),(.402,3),(.411,2),(.433,4),(.441,2)):
            rr=r*n; d.ellipse((c-rr,c-rr,c+rr,c+rr),outline=235,width=w)
        for i in range(96):
            a=i*math.tau/96
            line([point(.414,a),point(.428 if i%4==0 else .421,a)],3,215)
        for i in range(24):
            a=i*math.tau/24; r=.377
            # 几何符记不使用真实宗教文字；晶叶轮廓与分叉形成冰系语言。
            line([point(r-.012,a-.022),point(r+.012,a),point(r-.012,a+.022)],4)
            line([point(r-.014,a),point(r+.016,a)],3)
            if i%2==0:line([point(r,a-.021),point(r,a+.021)],3)
        for i in range(8):
            a=i*math.tau/8
            pts=[point(.46,a),point(.423,a-.018),point(.392,a),point(.423,a+.018),point(.46,a)]
            line(pts,4); line([point(.395,a),point(.455,a)],2,160)
        for i in range(12):
            a=i*30+4; r=.319*n; d.arc((c-r,c-r,c+r,c+r),a,a+22,fill=190,width=3)
    else:
        for r,w in ((.253,3),(.272,2),(.298,3)):
            rr=r*n;d.ellipse((c-rr,c-rr,c+rr,c+rr),outline=210,width=w)
        for i in range(6):
            a=i*math.tau/6
            line([point(0,a),point(.226,a)],5)
            line([point(.265,a),point(.175,a-.13),point(.058,a),point(.175,a+.13),point(.265,a)],3)
            for rr in (.09,.135,.185):
                line([point(rr-.028,a-.23),point(rr,a),point(rr-.028,a+.23)],3)
        for i in range(48):
            a=i*math.tau/48; p=point(.285,a);d.ellipse((p[0]-2,p[1]-2,p[0]+2,p[1]+2),fill=230)
        for i in range(8):
            a=i*math.tau/8+math.pi/8
            line([point(.312,a),point(.296,a-.022),point(.28,a),point(.296,a+.022),point(.312,a)],3)
    return np.asarray(im)

def make_textures():
    """制作八份透明遮罩和三份线性技术贴图，全部保留原始量纲与通道约定。"""
    save_sprite('RuneOuter',rune(False));save_sprite('RuneInner',rune(True))
    y,x=np.mgrid[0:N,0:N]/(N-1);u=x*2-1;v=y*2-1;r=np.sqrt(u*u+v*v)
    noise=frost_noise(N)
    mist=smooth((.94-r)/.50)*smooth((noise-.15)/.70)
    save_sprite('FrostMist',mist)
    trail=np.zeros((N,N),dtype=np.float32)
    for i in range(6):
        center=.5+.19*math.sin(i*1.8)*np.sin(x*math.tau*(1+i*.17)+i)
        trail+=np.exp(-((y-center)/(.008+i*.0025))**2)*(.80-i*.075)
    trail=(trail+.11*np.exp(-((y-.5)/.2)**2)*noise)*smooth(x/.055)*smooth((1-x)/.095)
    save_sprite('TrailStrip',trail)
    flash=np.exp(-r*r*55)+.62*np.exp(-np.abs(u)*75-np.abs(v)*3)+.62*np.exp(-np.abs(v)*75-np.abs(u)*3)
    save_sprite('ImpactFlash',flash*smooth((.96-r)/.13))
    im=Image.new('L',(N,N));d=ImageDraw.Draw(im)
    points=[(.50,.08),(.68,.47),(.56,.78),(.50,.93),(.34,.58),(.40,.29)]
    pts=[(int(a*N),int(b*N)) for a,b in points];d.polygon(pts,fill=225)
    d.line(pts+[pts[0]],fill=255,width=5); d.line([pts[0],(int(.49*N),int(.56*N)),pts[3]],fill=255,width=4)
    save_sprite('CrystalShard',np.asarray(im))
    flake=Image.new('L',(N,N));d=ImageDraw.Draw(flake)
    for i in range(6):
        a=i*math.tau/6
        def p(r,t=a):return (N/2+math.cos(t)*r*N,N/2+math.sin(t)*r*N)
        d.line([p(0),p(.40)],fill=255,width=8)
        for rr in (.15,.245,.335):d.line([p(rr-.05,a-.24),p(rr),p(rr-.05,a+.24)],fill=240,width=5)
    save_sprite('IceFlake',np.asarray(flake))
    cracks=Image.new('L',(N,N));d=ImageDraw.Draw(cracks);rng=np.random.default_rng(70015)
    for i in range(18):
        a=i*math.tau/18+rng.uniform(-.06,.06);last=np.array([N/2,N/2]);points=[tuple(last)]
        for step in range(1,12):
            angle=a+rng.uniform(-.18,.18); rr=step*.038*N
            pos=np.array([N/2+math.cos(angle)*rr,N/2+math.sin(angle)*rr]);points.append(tuple(pos))
            if step>2 and step%2==0:
                direction=angle+rng.choice([-1,1])*.55
                tip=pos+np.array([math.cos(direction),math.sin(direction)])*N*rng.uniform(.04,.08)
                d.line([tuple(pos),tuple(tip)],fill=155,width=2)
            last=pos
        d.line(points,fill=235,width=3 if i%3==0 else 2)
    crack=np.asarray(cracks,dtype=float)/255
    ground=(crack+.12*noise*smooth((.92-r)/.3))*smooth((.93-r)/.18)
    save_sprite('FrozenGround',ground)
    # 连续平铺曲面微裂纹，法线由线性高度中央差分求得，禁止用彩色示意图冒充法线。
    px=np.arange(N,dtype=np.float32)/N;xx,yy=np.meshgrid(px,px)
    h=np.zeros_like(xx)
    for ax,ay,phase in ((7,3,.2),(3,-9,.6),(13,5,.1),(5,17,.7)):
        dist=np.abs(np.sin(math.tau*(xx*ax+yy*ay+phase)))
        h=np.maximum(h,np.exp(-(dist/.075)**2))
    Image.fromarray((h*255).astype('uint8')).save(DEST/'T_DBA_IceArrow_CrackMask.png')
    dx=(np.roll(h,-1,1)-np.roll(h,1,1))*.6;dy=(np.roll(h,-1,0)-np.roll(h,1,0))*.6
    normal=np.stack((-dx,dy,np.ones_like(h)),axis=-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
    Image.fromarray(((normal*.5+.5)*255+.5).astype('uint8')).save(DEST/'T_DBA_IceArrow_IceNormal.png')
    orm=np.stack((np.ones_like(h),.13+.28*h,np.zeros_like(h)),axis=-1)
    Image.fromarray((orm*255+.5).astype('uint8')).save(DEST/'T_DBA_IceArrow_IceORM.png')

class Mesh:
    """原创OBJ三角网格；每面独立法线保证晶体硬边，单位厘米，不含材质/碰撞权威。"""
    def __init__(self): self.faces=[]
    def tri(self,a,b,c,uv=((0,0),(1,0),(.5,1))): self.faces.append((np.array([a,b,c],dtype=float),uv))
    def shard(self,base,axis,length,radius,sides=6):
        axis=np.array(axis,dtype=float);axis/=np.linalg.norm(axis)
        helper=np.array([0,0,1] if abs(axis[2])<.9 else [0,1,0],dtype=float)
        right=np.cross(axis,helper);right/=np.linalg.norm(right);up=np.cross(axis,right);base=np.array(base,dtype=float)
        rings=[]
        for s,k in ((0,.52),(.36,1),(.68,.72)):
            rings.append([base+axis*length*s+(right*math.cos(i*math.tau/sides)+up*math.sin(i*math.tau/sides))*radius*k for i in range(sides)])
        tip=base+axis*length
        for j in range(sides):
            k=(j+1)%sides;self.tri(base,rings[0][k],rings[0][j])
            for t in range(2):
                self.tri(rings[t][j],rings[t][k],rings[t+1][j]);self.tri(rings[t][k],rings[t+1][k],rings[t+1][j])
            self.tri(rings[-1][j],rings[-1][k],tip)
    def save(self,name):
        lines=['# SSLM原创冰箭网格源：厘米单位、独立UV与面法线；由UE导入形成StaticMesh。','o '+name]
        for vertices,uv in self.faces:
            for v in vertices:lines.append('v '+' '.join(f'{x:.6f}' for x in v))
            for t in uv:lines.append('vt '+' '.join(str(x) for x in t))
            normal=np.cross(vertices[1]-vertices[0],vertices[2]-vertices[0]);normal/=max(np.linalg.norm(normal),1e-12)
            for _ in range(3):lines.append('vn '+' '.join(f'{x:.6f}' for x in normal))
        for i in range(len(self.faces)):
            ids=[i*3+j+1 for j in range(3)];lines.append('f '+' '.join(f'{j}/{j}/{j}' for j in ids))
        (DEST/(name+'.obj')).write_text('\n'.join(lines)+'\n',encoding='utf-8')

def make_meshes():
    """主箭+卫星冰刃、单碎晶、落点冰冠和流带网格，各自独立便于Niagara组合。"""
    # 参考图为清晰单根长箭与后掠晶棱；旧版五根等长平行冰块会看成扇状光片。
    lance=Mesh();lance.shard((-95,0,0),(1,0,0),220,10)
    for i in range(4):
        a=i*math.tau/4
        lance.shard((-65,math.cos(a)*9,math.sin(a)*9),
                    (.85,math.cos(a)*.32,math.sin(a)*.32),85,6)
    lance.save('SM_DBA_IceArrow_CrystalLance')
    shard=Mesh();shard.shard((0,0,0),(0,0,1),75,12);shard.save('SM_DBA_IceArrow_CrystalShard')
    crown=Mesh();crown.shard((0,0,0),(0,0,1),105,14)
    for i in range(8):
        a=i*math.tau/8;crown.shard((math.cos(a)*17,math.sin(a)*17,0),(math.cos(a)*.65,math.sin(a)*.65,1),56+(i%3)*13,9)
    crown.save('SM_DBA_IceArrow_IceCrown')
    ribbon=Mesh()
    def pos(t,s):
        width=2+18*math.sin(math.pi*t)**.75
        return (-195+205*t,math.sin(t*math.tau)*9+s*width,math.cos(t*math.tau)*4)
    for i in range(24):
        a=i/24;b=(i+1)/24
        ribbon.tri(pos(a,-1),pos(a,1),pos(b,-1),((a,0),(a,1),(b,0)))
        ribbon.tri(pos(a,1),pos(b,1),pos(b,-1),((a,1),(b,1),(b,0)))
    ribbon.save('SM_DBA_IceArrow_TrailRibbon')

def main():
    """显式apply后执行；任何已有源文件均阻止覆盖，报告记录实际哈希而非声称已导入。"""
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--apply',action='store_true');args=p.parse_args()
    if not args.apply: print('SOURCE_OUTPUT',DEST);return
    if DEST.exists() and any(DEST.iterdir()):raise RuntimeError('IceArrow源目录已存在内容，拒绝覆盖')
    DEST.mkdir(parents=True,exist_ok=True);make_textures();make_meshes()
    entries=[{'file':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(DEST.iterdir()) if p.is_file()]
    report=ROOT/'Saved/Validation/IceArrow/SourceAssets.json';report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps({'source':'原创技术资产，非会话PNG导入','files':entries,'ue_imported':False},ensure_ascii=False,indent=2),encoding='utf-8')
    print('ICE_ARROW_SOURCE_CREATED',len(entries),str(DEST))
if __name__=='__main__':main()
