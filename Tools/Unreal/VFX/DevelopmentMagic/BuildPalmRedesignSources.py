# -*- coding: utf-8 -*-
"""项目表现层掌心审核源资产：厘米制可编辑OBJ、连续流场和冷雾图集。
仅输出SourceArt，不创建UE资产；原生导入由Monolith作者完成。固定种子便于重放，
火焰复用已存在的FireMage连续图集；这里不伪称运行时流体或物理体积模拟。
"""
from pathlib import Path
import json, math, hashlib, sys
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/PalmRedesign'
sys.path.insert(0,str(Path(__file__).parent))
from OrbV2Geometry import make_icosphere

def mesh(name,vertices,faces,uv,smooth):
    """输出逐角UV和法线；火壳与冰瓣均为有真实截面的三维表面。"""
    v=np.asarray(vertices,float); lines=['# SSLM palm concept review; units centimeters','o '+name]
    lines+=['v %.7f %.7f %.7f'%tuple(p) for p in v]
    normals=[]; corneruv=[]
    for face in faces:
        a,b,c=v[list(face)]; n=np.cross(b-a,c-a); n=n/max(np.linalg.norm(n),1e-9)
        for i in face:
            normals.append(n);corneruv.append(uv[i])
    if smooth:
        averaged=np.zeros_like(v)
        for face,n in zip(faces,normals[::3]):
            for i in face:averaged[i]+=n
        averaged/=np.maximum(np.linalg.norm(averaged,axis=1)[:,None],1e-8)
        normals=[averaged[i] for f in faces for i in f]
    lines+=['vt %.7f %.7f'%tuple(p) for p in corneruv]
    lines+=['vn %.7f %.7f %.7f'%tuple(p) for p in normals]
    for j,f in enumerate(faces):lines.append('f '+' '.join('%d/%d/%d'%(i+1,j*3+k+1,j*3+k+1) for k,i in enumerate(f)))
    p=OUT/(name+'.obj');p.write_text('\n'.join(lines)+'\n',encoding='ascii')
    return {'file':p.name,'triangles':len(faces),'min':v.min(axis=0).tolist(),'max':v.max(axis=0).tolist(),'finite':bool(np.isfinite(v).all())}

def curl(variant):
    """闭合卷焰壳：圆形截面沿弯曲轴收束，避免开放宽带形成空心纸片轮廓。"""
    vs=[];uv=[];fs=[]; rows=24;cols=10
    for iy in range(rows+1):
        t=iy/rows; theta=t*5.5+variant*.7
        radius=1.3+3.5*math.sin(t*math.pi)**1.1
        width=.025+(3.0+1.7*math.sin(t*math.pi*2.8+variant*.4))*(math.sin(math.pi*t)**.65)
        for ix in range(cols+1):
            s=ix/cols; a=s*math.pi*2
            fold=1+.24*math.sin(a*3+t*9+variant)
            x=radius*math.cos(theta)+width*math.cos(a)*fold
            y=radius*math.sin(theta)+width*.85*math.sin(a)*fold
            z=1+t*(27-variant*2)+.7*math.sin(a*2+t*8)*math.sin(math.pi*t)
            vs.append((x,y,z));uv.append((s,1-t))
    for y in range(rows):
        for x in range(cols):
            a=y*(cols+1)+x;b=a+1;c=a+cols+1;d=c+1
            fs.extend([(a,b,c),(b,d,c)])
    for j in range(1,cols-1):fs.extend([(0,j+1,j),(rows*(cols+1),rows*(cols+1)+j,rows*(cols+1)+j+1)])
    return mesh('SM_DBA_PalmFireCurl_'+chr(65+variant),vs,fs,uv,True)

def petal(variant):
    """不规则六棱弯晶，具有厚度与错列切面；末端回卷而不是均匀叶片。"""
    vs=[];uv=[];fs=[];rows=7;cols=6
    for iy in range(rows+1):
        t=iy/rows; width=.015+2.1*math.sin(math.pi*t)**.8
        bend=1.1+6.8*math.sin(t*math.pi*.82)+1.6*t*t
        for ix in range(cols+1):
            s=ix/cols;a=s*math.pi*2+(iy%2)*.18
            irregular=1+.16*math.sin(iy*12.7+(ix%cols)*4.9+variant)
            vs.append((bend+math.cos(a)*width*.68*irregular,math.sin(a)*width*irregular+1.1*math.sin(t*4+variant)*t,1+t*(20+variant*1.8)+.4*math.sin(a*2+iy)*math.sin(math.pi*t)))
            uv.append((s,1-t))
    for y in range(rows):
        for x in range(cols):
            a=y*(cols+1)+x;b=a+1;c=a+cols+1;d=c+1
            fs.extend([(a,b,c),(b,d,c)])
    for j in range(1,cols-1):fs.extend([(0,j+1,j),(rows*(cols+1),rows*(cols+1)+j,rows*(cols+1)+j+1)])
    return mesh('SM_DBA_PalmIcePetal_'+chr(65+variant),vs,fs,uv,False)

def noise(u,v,t=0):
    """解析连续低频卷流，时间使用周期函数而不是逐帧随机，避免序列跳动。"""
    q=u+.075*np.sin(v*14+np.sin(t));r=v+.09*np.cos(u*11+np.cos(t))
    return np.clip(.5+.22*np.sin(q*29+np.sin(r*15)+t)+.16*np.cos(r*37-q*8-t)+.08*np.sin(q*71+r*61+2*t),0,1)

def texture(name,rgba):
    """输出线性数据PNG。这里只编写资产生成程序，不改动概念图或任何原图像。"""
    p=OUT/(name+'.png');Image.fromarray(np.uint8(np.clip(rgba,0,1)*255),'RGBA').save(p)
    return {'file':p.name,'size':[rgba.shape[1],rgba.shape[0]],'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}

def run():
    OUT.mkdir(parents=True,exist_ok=True);manifest={'units':'cm','proceduralAuthoring':True,'runtimeFluids':False,'meshes':[],'textures':[]}
    for i in range(3):manifest['meshes'] +=[curl(i),petal(i)]
    vs,fs=make_icosphere(1,3.0)
    vs=[(x*(1+.07*math.sin(y*2)),y*.93,z*1.18) for x,y,z in vs]
    uv=[(.5+math.atan2(y,x)/(2*math.pi),.5-z/7.1) for x,y,z in vs]
    manifest['meshes'].append(mesh('SM_DBA_PalmIceCore',vs,fs,uv,False))
    v,u=np.mgrid[0:1:512j,0:1:512j];n=noise(u,v);du=np.gradient(n,axis=1);dv=np.gradient(n,axis=0)
    manifest['textures'].append(texture('T_DBA_PalmFire_Flow',np.stack([.5+dv*4,.5-du*4,n,np.ones_like(n)],-1)))
    v,u=np.mgrid[0:1:1024j,0:1:1024j];n=noise(u,v)
    fracture=np.power(np.clip(1-np.abs(np.sin(u*24+2*np.sin(v*13)))*8,0,1),2)
    thickness=np.clip(np.sin(v*math.pi)*np.sin(u*math.pi),0,1)
    manifest['textures'].append(texture('T_DBA_PalmIce_Surface',np.stack([n*.22,thickness,.13+.16*n,fracture*.5],-1)))
    dx=np.gradient(n,axis=1)*4;dy=np.gradient(n,axis=0)*4
    manifest['textures'].append(texture('T_DBA_PalmIce_Normal',np.stack([.5-dx,.5-dy,np.ones_like(n),np.ones_like(n)],-1)))
    atlas=np.zeros((1024,1024,4));v,u=np.mgrid[0:1:256j,0:1:256j]
    for k in range(16):
        t=2*math.pi*k/16;n=noise(u,v,t);dist=((u-.5)/.45)**2+((v-.5)/.42)**2
        a=np.clip(1-dist,0,1)**2*np.clip((n-.12)*1.5,0,1)
        # 边界零密度，Mip不会把相邻格的浓雾带进透明边缘。
        cell=np.stack([n,n,n,a],-1);y=(k//4)*256;x=(k%4)*256;atlas[y:y+256,x:x+256]=cell
    manifest['textures'].append(texture('T_DBA_PalmMist_Flipbook',atlas))
    assert all(m['finite'] and m['max'][2]-m['min'][2]>5 for m in manifest['meshes'])
    (OUT/'SourceManifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
    print(json.dumps({'meshes':len(manifest['meshes']),'textures':len(manifest['textures']),'directory':str(OUT)}))
if __name__=='__main__':run()
