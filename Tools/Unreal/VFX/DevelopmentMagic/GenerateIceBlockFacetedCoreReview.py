# -*- coding: utf-8 -*-
"""《神兽联盟》冰块多面晶核OBJ审核源网格：非长条冰箭。只创建新的独立源文件。"""
import math,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
DEST=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/OrbProjectile/Review/V1'
DEST.mkdir(parents=True,exist_ok=True)
TARGET=DEST/'SM_DBA_IceBlock_FacetedCore_Review.obj'
if TARGET.exists():raise RuntimeError('冰块审核网格源文件已存在，拒绝覆盖 '+str(TARGET))
# +X是弹体前向，外形大致长58×宽53×高59cm；每块面具有独立法线与非均匀晶面
corners=[
(-.86,-.93,-.90),(.91,-.84,-.87),(.97,.90,-.94),(-.91,.94,-.91),
(-.95,-.83,.84),(.94,-.97,.98),(.84,.93,.90),(-.82,.90,1.01)]
faces=[
 ([0,3,2,1],(0,0,-1.27)), ([4,5,6,7],(0,0,1.28)),
 ([0,1,5,4],(0,-1.27,0)), ([3,7,6,2],(0,1.26,0)),
 ([0,4,7,3],(-1.23,0,0)), ([1,2,6,5],(1.25,0,0))
]
# 微弱破坏对称，避免像标准方盒
def shape(v):return (v[0]*29.0,v[1]*27.0,v[2]*29.0)
verts=[shape(x) for x in corners]+[shape(x[1]) for x in faces]
triangles=[]
for i,(edge,_) in enumerate(faces):
  apex=8+i
  for j in range(4):
    a,b,c=edge[j],edge[(j+1)%4],apex
    def sub(x,y):return tuple(x[k]-y[k] for k in range(3))
    def cross(x,y):return (x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0])
    pa,pb,pc=verts[a],verts[b],verts[c]
    n=cross(sub(pb,pa),sub(pc,pa))
    m=tuple((pa[k]+pb[k]+pc[k])/3 for k in range(3))
    if sum(n[k]*m[k] for k in range(3))<0:b,c=c,b;n=tuple(-v for v in n)
    l=math.sqrt(sum(v*v for v in n));n=tuple(v/l for v in n)
    triangles.append(((a,b,c),n))
out=['# SSLM IceBlock Faceted Core V1 / independent UE5.8 review mesh','o SM_DBA_IceBlock_FacetedCore_Review']
out += ['v %.6f %.6f %.6f'%x for x in verts]
uvs=[];normals=[]
for ids,n in triangles:
  main=max(range(3),key=lambda k:abs(n[k]));inds=[k for k in range(3) if k!=main]
  # Project local face UVs; no material bitmap baked into geometry
  for ix in ids:
    v=verts[ix];uvs.append((0.5+v[inds[0]]/75,0.5+v[inds[1]]/75))
  normals.append(n)
out += ['vt %.6f %.6f'%x for x in uvs]
out += ['vn %.6f %.6f %.6f'%x for x in normals]
out += ['s off','usemtl IceBlockPBR']
for k,(ids,n) in enumerate(triangles):
  tri=[]
  for l,idx in enumerate(ids):
    tri.append('%d/%d/%d'%(idx+1,k*3+l+1,k+1))
  out.append('f '+' '.join(tri))
TARGET.write_text('\n'.join(out)+'\n',encoding='ascii')
manifest={'source':str(TARGET),'sha256':hashlib.sha256(TARGET.read_bytes()).hexdigest(),'vertices':len(verts),'triangles':len(triangles),'units':'centimeters','maxExtentCm':[max(abs(v[i]) for v in verts) for i in range(3)],'approved':False}
(DEST/'IceBlockFacetedCore_ReviewManifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('REVIEW_ICE_BLOCK_OBJ',json.dumps(manifest,ensure_ascii=False))
