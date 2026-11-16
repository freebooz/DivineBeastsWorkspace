#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM项目层20种通用VFX的可编辑源几何作者。

只输出厘米制OBJ、确认稿归档和制作合同，不生成UE资产、不执行玩法。
曲面具备连续顶点法线及UV；主体/边缘/辅助层独立，供Monolith导入与Niagara复用。
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import shutil

ROOT = Path(__file__).resolve().parents[4]
PACK = ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core'
DEST = PACK/'SourceArt/VFX/CommonSkillVFX/V1'
TAU = math.tau


def add(a,b): return tuple(x+y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def mul(a,s): return tuple(x*s for x in a)
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def norm(a): return mul(a,1/max(math.sqrt(sum(x*x for x in a)),1e-12))
def rotate(p,angle): return (p[0]*math.cos(angle)-p[1]*math.sin(angle),p[0]*math.sin(angle)+p[1]*math.cos(angle),p[2])


class Mesh:
    """厘米制平滑曲面集合；法线来自参数曲面切向，避免逐面晶化。"""
    def __init__(self): self.vertices=[];self.faces=[]
    def surface(self,fn,nu=80,nv=8):
        offset=len(self.vertices)
        for i in range(nu+1):
            u=i/nu
            for j in range(nv+1):
                v=j/nv;e=.0001
                du=sub(fn(min(1,u+e),v),fn(max(0,u-e),v))
                dv=sub(fn(u,min(1,v+e)),fn(u,max(0,v-e)))
                normal=norm(cross(du,dv))
                self.vertices.append((fn(u,v),normal,(u,v)))
        for i in range(nu):
            for j in range(nv):
                a=offset+i*(nv+1)+j;b=a+nv+1
                self.faces.extend(((a,b,a+1),(a+1,b,b+1)))
    def tube(self,path,radius=2,steps=80,sides=8,flatten=1):
        def fn(u,v):
            p=path(u);tangent=norm(sub(path(min(1,u+.0001)),path(max(0,u-.0001))))
            basis=(0,0,1) if abs(tangent[2])<.93 else (0,1,0)
            side=norm(cross(tangent,basis));up=norm(cross(side,tangent))
            r=radius(u) if callable(radius) else radius
            return add(p,add(mul(side,math.cos(v*TAU)*r),mul(up,math.sin(v*TAU)*r*flatten)))
        self.surface(fn,steps,sides)
    def ring(self,r,z=3,width=2,center=(0,0,0),start=0,end=TAU):
        self.tube(lambda u:add(center,(r*math.cos(start+(end-start)*u),r*math.sin(start+(end-start)*u),z)),width,96,8)
    def leaf(self,center,angle=0,length=65,width=16,tilt=.4):
        def fn(u,v):
            t=.003+.994*u
            w=width*max(.002,math.sin(math.pi*t))**.8
            p=((t-.5)*length,(v*2-1)*w,math.sin(math.pi*t)*length*.15-(v*2-1)**2*3)
            p=(p[0]*math.cos(tilt)+p[2]*math.sin(tilt),p[1],-p[0]*math.sin(tilt)+p[2]*math.cos(tilt))
            return add(center,rotate(p,angle))
        self.surface(fn,36,10)
    def save(self,path):
        lines=['# SSLM CommonSkillVFX original editable geometry; units: centimeters','o '+path.stem]
        lines += ['v '+' '.join(f'{x:.6f}' for x in p) for p,_,_ in self.vertices]
        lines += ['vt '+' '.join(f'{x:.6f}' for x in uv) for _,_,uv in self.vertices]
        lines += ['vn '+' '.join(f'{x:.7f}' for x in n) for _,n,_ in self.vertices]
        lines += ['f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in f) for f in self.faces]
        path.write_text('\n'.join(lines)+'\n',encoding='utf8')


def catalog():
    """稳定项目内容键，不是英雄AbilityId；运行逻辑ID由相同键派生。"""
    names=[('Attack_Slash','弧刃斩击'),('Attack_Projectile','定向飞弹'),('Attack_AreaPulse','范围冲击'),('Attack_Pierce','直线贯穿'),
           ('Defense_Shield','单体护盾'),('Defense_Wall','定向盾墙'),('Defense_Parry','瞬时格挡'),('Defense_Dome','群体庇护'),
           ('Control_Root','束缚定身'),('Control_Stun','眩晕标记'),('Control_SlowArea','区域减速'),('Control_KnockUp','击飞反馈'),
           ('Healing_Burst','瞬时恢复'),('Healing_Regen','持续再生'),('Healing_Area','治疗领域'),('Healing_Link','恢复连线'),
           ('Movement_Dash','短距冲刺'),('Movement_Blink','瞬间闪现'),('Movement_Leap','跳跃落点'),('Movement_Haste','加速疾行')]
    colors={'Attack':'F16B46','Defense':'B5CADC','Control':'409CE5','Healing':'47B986','Movement':'68D7B4'}
    for_index=[]
    for i,(key,zh) in enumerate(names):
        cat=key.split('_')[0]
        behavior='Attached'
        if key in ('Attack_Slash','Defense_Parry','Healing_Burst','Movement_Blink'):behavior='Instant'
        elif key=='Attack_Projectile':behavior='Projectile'
        elif key in ('Attack_Pierce','Healing_Link'):behavior='Beam'
        elif key in ('Attack_AreaPulse','Control_SlowArea','Healing_Area'):behavior='Area'
        elif cat=='Defense':behavior='Shield'
        elif cat=='Movement' or key=='Control_KnockUp':behavior='Trail'
        duration=.8 if key in ('Attack_Slash','Defense_Parry','Healing_Burst') else 1.2 if cat=='Attack' or cat=='Movement' or key=='Control_KnockUp' else 3.
        for_index.append(dict(key=key,nameZh=zh,index=i,category=cat,referenceSRGB=colors[cat],behavior=behavior,duration=duration,
            definitionId='presentation.dba.common.vfx.'+key.replace('_','.').lower()+'@1',
            requiredShapeZh=zh+'：确认稿主体轮廓、独立边缘、少量辅助；无噪声和碎粒堆积',
            stages=['Start','Peak','Sustain' if duration>=3 else 'Release','End','Cancel'],manualAccepted=False))
    return for_index


def build_parts(key):
    """20种真实不同的三维轮廓；Core、Rim、Accent共享母材质而不合并生命周期。"""
    core,rim,acc=Mesh(),Mesh(),Mesh()
    def line(mesh,path,w=3,flat=1):mesh.tube(path,w,80,8,flat)
    def glyph(mesh,center,scale=1):
        for a in range(4):mesh.leaf(add(center,rotate((19*scale,0,0),a*math.pi/2)),a*math.pi/2,40*scale,8*scale,.05)
    def chevrons(mesh,r=210,z=5,count=6):
        for j in range(count):
            a=j*TAU/count
            for side in (-1,1):line(mesh,lambda t,a=a,side=side:rotate((r-12+24*t,side*16*(1-t),z+2*math.sin(math.pi*t)),a),1.8)
    if key=='Attack_Slash':
        path=lambda u:(60+165*math.cos(-1.3+2.9*u),165*math.sin(-1.3+2.9*u),72+70*math.sin(math.pi*u))
        core.tube(path,lambda u:1.5+24*math.sin(math.pi*u)**.8,112,10,.12)
        line(rim,lambda u:add(path(u),(0,0,3)),lambda u:1+2.5*math.sin(math.pi*u),.5)
        line(acc,lambda u:(45+185*math.cos(-1.45+2.5*u),185*math.sin(-1.45+2.5*u),62+65*math.sin(math.pi*u)),lambda u:.5+5*math.sin(math.pi*u),.12)
    elif key in ('Attack_Projectile','Attack_Pierce'):
        length=145 if key.endswith('Projectile') else 480
        core.surface(lambda u,v:(length*(u-.25),math.cos(TAU*v)*(1+13*math.sin(math.pi*(.002+.996*u))**1.3),105+math.sin(TAU*v)*(1+9*math.sin(math.pi*(.002+.996*u))**1.3)),100,16)
        for s in (-1,1):
            line(rim,lambda u,s=s:(length*(u-.25),s*(2+13*math.sin(math.pi*u)),105+3*math.sin(TAU*u)),1.5)
            line(acc,lambda u,s=s:(-180+260*u,s*(5+16*math.sin(math.pi*u)),105+12*math.sin(math.pi*u)),lambda u:.6+3*math.sin(math.pi*u),.16)
    elif key=='Attack_AreaPulse':
        core.ring(150,20,13);rim.ring(213,4,2)
        for j in range(4):
            a=j*math.pi/2
            line(core,lambda u,a=a:rotate((35+130*u,0,10+45*math.sin(math.pi*u)),a),lambda u:1+8*math.sin(math.pi*u),.15)
        chevrons(acc)
    elif key in ('Defense_Shield','Defense_Dome'):
        dome=key.endswith('Dome');rx=210 if dome else 95;ry=rx;rz=220 if dome else 125;cz=0 if dome else 125
        lo=.01;hi=math.pi/2-.005 if dome else math.pi-.01
        core.surface(lambda u,v:(rx*math.sin(lo+(hi-lo)*u)*math.cos(v*TAU),ry*math.sin(lo+(hi-lo)*u)*math.sin(v*TAU),cz+rz*math.cos(lo+(hi-lo)*u)),64,96)
        for j in range(3):
            a=j*TAU/3
            line(rim,lambda u,a=a:(rx*math.sin((.02+(math.pi-.04)*u))*math.cos(a+.35*math.sin(math.pi*u)),ry*math.sin((.02+(math.pi-.04)*u))*math.sin(a+.35*math.sin(math.pi*u)),cz+rz*math.cos(.02+(math.pi-.04)*u)) if not dome else (rx*math.cos(math.pi*u)*math.cos(a),rx*math.cos(math.pi*u)*math.sin(a),rz*math.sin(math.pi*u)+3),3)
        acc.ring(rx,3,1.6);glyph(acc,(0,0,5),.75)
    elif key=='Defense_Wall':
        core.surface(lambda u,v:(120-38*math.sin(math.pi*u),-155+310*u,8+220*v),72,36)
        for z in (8,228):line(rim,lambda u,z=z:(120-38*math.sin(math.pi*u),-155+310*u,z),4)
        for y in (-155,155):line(rim,lambda u,y=y:(120,y,8+220*u),5)
        for s in (-1,1):line(acc,lambda u,s=s:(79,-120+240*u,110+s*52*math.sin(math.pi*u)),2)
    elif key=='Defense_Parry':
        core.surface(lambda u,v:(80+15*math.sin(math.pi*u),(.5-u)*135,105+(.5-v)*135),48,48)
        for r in (45,65):line(rim,lambda u,r=r:(82,r*math.cos(TAU*u),105+r*math.sin(TAU*u)),2.4)
        line(acc,lambda u:(86,85*math.cos(-1+2*u),105+85*math.sin(-1+2*u)),3)
    elif key=='Control_Root':
        for j in range(3):line(core,lambda u,j=j:(45*math.cos(u*TAU*.8+j*TAU/3),45*math.sin(u*TAU*.8+j*TAU/3),12+75*u),5,.35)
        rim.ring(65,3,2);rim.ring(30,225,2)
        for s in (-1,1):line(acc,lambda u,s=s:(s*11,0,218+19*u),3)
        line(acc,lambda u:(11*math.cos(math.pi*u),0,236+11*math.sin(math.pi*u)),3)
    elif key=='Control_Stun':
        for i in range(3):core.ring(46,235,3,start=i*TAU/3+.08,end=(i+1)*TAU/3-.3)
        line(rim,lambda u:(0,0,215+48*u),lambda u:1+5*math.sin(math.pi*u),.8)
        acc.ring(60,3,1.6);glyph(acc,(0,0,5),.55)
    elif key=='Control_SlowArea':
        rim.ring(220,3,2);chevrons(acc,220)
        for i in range(6):
            a=i*TAU/6
            line(core,lambda u,a=a:( (205-35*u)*math.cos(a+.5*u),(205-35*u)*math.sin(a+.5*u),8+17*math.sin(math.pi*u)),lambda u:2+10*math.sin(math.pi*u),.22)
    elif key=='Control_KnockUp':
        line(core,lambda u:(45*math.cos(u*TAU*1.3),45*math.sin(u*TAU*1.3),10+240*u),5,.35)
        rim.ring(55,3,1.8)
        line(acc,lambda u:(0,0,200+65*u),2)
        for s in (-1,1):line(acc,lambda u,s=s:(s*18*(1-u),0,240+25*u),2)
    elif key in ('Healing_Burst','Healing_Regen'):
        if key.endswith('Burst'):
            glyph(core,(0,0,120),1.1);glyph(acc,(0,0,240),.6)
        else:
            for i in range(3):core.leaf((60*math.cos(i*2.2),60*math.sin(i*2.2),65+i*60),i*2.2,70,18,-.8)
            glyph(acc,(0,0,5),.65)
        for s in (-1,1):line(rim,lambda u,s=s:(s*68*math.cos(u*math.pi*1.4),s*68*math.sin(u*math.pi*1.4),15+210*u),lambda u:1+8*math.sin(math.pi*u),.14)
    elif key=='Healing_Area':
        rim.ring(220,3,2)
        for i in range(4):
            a=i*TAU/4
            core.leaf((185*math.cos(a),185*math.sin(a),30),a,105,22,.65)
            glyph(acc,(220*math.cos(a),220*math.sin(a),5),.5)
    elif key=='Healing_Link':
        line(core,lambda u:(-200+400*u,15*math.sin(TAU*u),100+22*math.sin(math.pi*u)),lambda u:1.5+6*math.sin(math.pi*u),.3)
        line(rim,lambda u:(-200+400*u,10*math.sin(TAU*u+1),100+30*math.sin(math.pi*u)),1.4)
        glyph(acc,(200,0,105),.8)
    elif key=='Movement_Dash':
        for s in (-1,1):line(core,lambda u,s=s:(-360+440*u,s*(25+16*math.sin(TAU*u)),15+42*math.sin(math.pi*u)),lambda u:.7+8*math.sin(math.pi*u),.16)
        rim.ring(55,3,1.5,center=(80,0,0));rim.ring(45,3,1,center=(-360,0,0))
        for s in (-1,1):line(acc,lambda u,s=s:(50+30*u,s*20*(1-u),5),2)
    elif key=='Movement_Blink':
        for x in (0,400):
            line(core,lambda u,x=x:(x,65*math.cos(TAU*u),120+115*math.sin(TAU*u)),2.5)
            rim.ring(65,3,1.5,center=(x,0,0));glyph(acc,(x,0,5),.65)
    elif key=='Movement_Leap':
        line(core,lambda u:(-240+450*u,0,8+235*math.sin(math.pi*u)),lambda u:.6+4*math.sin(math.pi*u),.4)
        rim.ring(60,3,1.5,center=(210,0,0));rim.ring(35,3,1,center=(-240,0,0))
        for j in range(3):
            a=j*TAU/3
            line(acc,lambda u,a=a:add((210,0,4),rotate((40+20*u,16*(1-u),0),a)),2)
    elif key=='Movement_Haste':
        for s in (-1,1):
            line(core,lambda u,s=s:(-210+250*u,s*(18+22*math.sin(math.pi*u)),15+30*math.sin(math.pi*u)),lambda u:.8+7*math.sin(math.pi*u),.12)
            line(rim,lambda u,s=s:(-160+190*u,s*(23+13*math.sin(math.pi*u)),30+15*math.sin(math.pi*u)),1.4)
        line(acc,lambda u:(-180+220*u,0,3+2*math.sin(math.pi*u)),lambda u:.5+2*math.sin(math.pi*u))
    else:raise ValueError('未知模板：'+key)
    return dict(Core=core,Rim=rim,Accent=acc)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
    if not args.apply:print(json.dumps(catalog(),ensure_ascii=False,indent=2));return
    if (DEST/'Sources.json').exists():raise RuntimeError('已存在源合同，拒绝覆盖；使用新修订或精确修复')
    (DEST/'Meshes').mkdir(parents=True,exist_ok=True);(DEST/'Concepts').mkdir(exist_ok=True)
    preview=ROOT/'Saved/ArtPreviews/CommonSkillVFX_20261018'
    verification=json.loads((preview/'Verification.json').read_text(encoding='utf-8-sig'))
    for r in verification:
        p=preview/Path(r['Path']).name
        if hashlib.sha256(p.read_bytes()).hexdigest().upper()!=r['SHA256']:raise RuntimeError('原稿SHA不一致')
        shutil.copy2(p,DEST/'Concepts'/p.name)
    rows=catalog()
    for row in rows:
        row['meshes']=[]
        for part,mesh in build_parts(row['key']).items():
            p=DEST/'Meshes'/('SM_DBA_Common_'+row['key']+'_'+part+'.obj');mesh.save(p)
            row['meshes'].append(dict(part=part,file=str(p.relative_to(DEST)).replace('\\','/'),vertices=len(mesh.vertices),triangles=len(mesh.faces),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    (DEST/'Sources.json').write_text(json.dumps(dict(schemaVersion=1,scope='Project reusable visual content',units='centimeters',paletteAuthority='Docs/Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md',concepts=verification,templates=rows,nativeCreated=False),ensure_ascii=False,indent=2),encoding='utf8')
    print('COMMON_VFX_SOURCE_READY templates=20 meshes=60 concepts=5')


if __name__=='__main__':main()
