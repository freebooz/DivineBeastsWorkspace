# -*- coding: utf-8 -*-
"""SSLM V2返工源几何：圆润火核与闭合不规则晶核，单位厘米。
只有OBJ源文件由此生成，.uasset必须由UE原生导入；不修改V1、碰撞或Gameplay。
"""
import math, json, hashlib
from pathlib import Path

def normalized(v):
    """三维单位向量；零向量属于错误输入。"""
    s=math.sqrt(sum(x*x for x in v))
    if s<1e-12:raise ValueError('不能归一化零向量')
    return tuple(x/s for x in v)

def make_icosphere(level=1,radius=1.0):
    """从二十面体细分球面，每条共享边只生成一个中点，以保持封闭拓扑。"""
    a=(1+math.sqrt(5))/2
    verts=[normalized(v) for v in [(-1,a,0),(1,a,0),(-1,-a,0),(1,-a,0),(0,-1,a),(0,1,a),(0,-1,-a),(0,1,-a),(a,0,-1),(a,0,1),(-a,0,-1),(-a,0,1)]]
    faces=[(0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),(1,5,9),(5,11,4),(11,10,2),(10,7,6),(7,1,8),(3,9,4),(3,4,2),(3,2,6),(3,6,8),(3,8,9),(4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1)]
    for _ in range(level):
        cache={}
        def mid(a,b):
            edge=tuple(sorted((a,b)))
            if edge not in cache:
                cache[edge]=len(verts)
                verts.append(normalized(tuple((verts[a][k]+verts[b][k])/2 for k in range(3))))
            return cache[edge]
        refined=[]
        for a,b,c in faces:
            ab,bc,ca=mid(a,b),mid(b,c),mid(c,a)
            refined.extend([(a,ab,ca),(b,bc,ab),(c,ca,bc),(ab,bc,ca)])
        faces=refined
    return [tuple(x*radius for x in v) for v in verts],faces

def make_ice_crystal():
    """不规则多面晶块：保持块状体积但消除立方盒外轮廓，不沿用八角方盒。"""
    verts,faces=make_icosphere(1,1.0)
    out=[]
    for x,y,z in verts:
        radius=29*(1+.095*math.sin(4*x+2*y-3*z)+.055*math.cos(7*y+3*z))
        out.append((x*radius*1.17+.9*z,y*radius*.94,z*radius*1.05))
    return out,faces

def write_obj(path,verts,faces,smooth=False):
    """输出带法线与球形UV的OBJ；冰块面法线独立，火核法线平滑。"""
    path=Path(path)
    if path.exists():raise FileExistsError('禁止覆盖已有源模型 '+str(path))
    text=['# SSLM V2 independent review geometry','o '+path.stem]
    text+=['v %.6f %.6f %.6f'%v for v in verts]
    uv=[];normals=[]
    for face in faces:
        a,b,c=[verts[i] for i in face]
        p=tuple(b[k]-a[k] for k in range(3));q=tuple(c[k]-a[k] for k in range(3))
        n=normalized((p[1]*q[2]-p[2]*q[1],p[2]*q[0]-p[0]*q[2],p[0]*q[1]-p[1]*q[0]))
        vals=[]
        for idx in face:
            v=normalized(verts[idx]);vals.append((.5+math.atan2(v[1],v[0])/(2*math.pi),math.acos(max(-1,min(1,v[2])))/math.pi))
            normals.append(v if smooth else n)
        if max(v[0] for v in vals)-min(v[0] for v in vals)>.5:vals=[(u+1 if u<.5 else u,v) for u,v in vals]
        uv.extend(vals)
    text+=['vt %.7f %.7f'%v for v in uv]
    text+=['vn %.7f %.7f %.7f'%v for v in normals]
    text+=['s 1' if smooth else 's off']
    for j,face in enumerate(faces):text.append('f '+' '.join('%d/%d/%d'%(idx+1,j*3+k+1,j*3+k+1) for k,idx in enumerate(face)))
    path.write_text('\n'.join(text)+'\n',encoding='ascii')
    return {'name':path.name,'vertices':len(verts),'triangles':len(faces),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}

if __name__=='__main__':
    root=Path(__file__).resolve().parents[4]
    dest=root/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/OrbProjectile/Review/V2'
    dest.mkdir(parents=True,exist_ok=True)
    v,f=make_icosphere(3,28);a=write_obj(dest/'SM_DBA_FireCore_V2.obj',v,f,True)
    v,f=make_ice_crystal();b=write_obj(dest/'SM_DBA_IceCrystal_V2.obj',v,f,False)
    (dest/'GeometryManifest.json').write_text(json.dumps({'units':'cm','manualAccepted':False,'assets':[a,b]},indent=2),encoding='utf8')
    print('ORBV2_SOURCE_GEOMETRY',a,b)
