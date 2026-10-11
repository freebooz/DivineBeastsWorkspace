#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成项目层蓝紫蝶与银蓝鱼的可复用源美术，必须由独立 Blender 后台进程执行。

职责与边界：只拥有 DBAPresentationPack_Core/SourceArt/Fauna 内本生成器产物。
输入：--texture-size=512/1024、--render 可选真实 Cycles 预览。
输出：真实 FBX、包含两独立场景的 blend、BaseColor/Roughness/Normal PNG、哈希及重导入报告。
模型局部坐标 X 前/Y 左右/Z 上，米制源文件；UE 只转换一次厘米。
顶点色 FaunaColorMotionWeight 的 RGB 是部位颜色，A 是翼/尾形变权重。
此工具不启动 UE、不创建 uasset、不执行玩法；技术检查不代表人工艺术验收通过。
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib
import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Fauna"
GENERATOR = "Tools/Blender/Fauna/GenerateFaunaModels.py"
COLOR_LAYER = "FaunaColorMotionWeight"
HINGE_Y_M, HINGE_Z_M = 0.0025, 0.0012
FORE = [(0.014,0.0035),(0.033,0.017),(0.046,0.037),(0.039,0.055),
        (0.024,0.063),(0.009,0.057),(-0.003,0.041),(-0.010,0.014),(-0.008,0.004)]
HIND = [(0.002,0.005),(-0.003,0.025),(-0.018,0.045),(-0.035,0.047),
        (-0.050,0.036),(-0.050,0.021),(-0.035,0.010),(-0.016,0.004)]

def smooth01(value):
    """零至一平滑插值：用于翼根及尾部权重，边界不产生突变。"""
    value = np.clip(value, 0.0, 1.0)
    return value * value * (3.0 - 2.0 * value)

def motion_butterfly(position):
    """只有翼片调用；身体、足和触角通过显式零权重保持稳定。"""
    return float(smooth01((abs(position[1])-HINGE_Y_M)/(0.018-HINGE_Y_M)))

def motion_fish(position):
    """头部 X>=3.5cm 为零，向负X的尾部逐渐增强，X<=-12cm 为一。"""
    return float(smooth01((0.035-position[0])/0.155))

def catmull_closed(points, subdivisions=5):
    """闭合轮廓采用 Catmull-Rom 插值；保留可审阅的解剖轮廓控制点。"""
    points = np.asarray(points, dtype=float)
    result = []
    for i in range(len(points)):
        p0,p1,p2,p3 = (points[(i+j) % len(points)] for j in (-1,0,1,2))
        for k in range(subdivisions):
            t = k/subdivisions
            result.append(0.5*((2*p1)+(-p0+p2)*t+
                          (2*p0-5*p1+4*p2-p3)*t*t+
                          (-p0+3*p1-3*p2+p3)*t*t*t))
    return np.asarray(result)

class MeshBuilder:
    """积累三角网格及逐顶点UV/RGBA；不创建共面反向面，双面由材质负责。"""
    def __init__(self, species):
        self.species = species
        self.positions, self.uvs, self.colors, self.faces = [], [], [], []
        self.parts = []

    def vertex(self, position, uv, rgb, weight=0.0):
        self.positions.append(tuple(float(v) for v in position))
        self.uvs.append(tuple(float(v) for v in uv))
        self.colors.append(tuple(float(v) for v in rgb)+(float(np.clip(weight,0,1)),))
        return len(self.positions)-1

    def triangle(self, a,b,c):
        self.faces.append((a,b,c))

    def quad(self,a,b,c,d):
        self.triangle(a,b,c); self.triangle(a,c,d)

    def mark_part(self, name, begin):
        self.parts.append({"name":name,"firstVertex":begin,"vertexCount":len(self.positions)-begin})

    def ellipse(self, name, center, radii, color, weight=None, segments=20, rings=10):
        """封闭椭球用于胸腹、头和眼；极点采用单顶点，避免零面积三角形。"""
        start=len(self.positions); center=np.asarray(center)
        tail=self.vertex(center+np.asarray((-radii[0],0,0)),(0.05,0.045),color,0 if weight is None else weight(center))
        ring_ids=[]
        for i in range(1,rings):
            angle=math.pi*i/rings
            ids=[]
            for j in range(segments+1):
                q=2*math.pi*j/segments
                p=center+np.asarray((-radii[0]*math.cos(angle),
                     radii[1]*math.sin(angle)*math.cos(q),radii[2]*math.sin(angle)*math.sin(q)))
                ids.append(self.vertex(p,(0.05+0.90*j/segments,0.025+0.045*i/rings),color,0 if weight is None else weight(p)))
            ring_ids.append(ids)
        nose=self.vertex(center+np.asarray((radii[0],0,0)),(0.95,0.045),color,0 if weight is None else weight(center))
        for j in range(segments):
            self.triangle(tail,ring_ids[0][j+1],ring_ids[0][j])
            self.triangle(nose,ring_ids[-1][j],ring_ids[-1][j+1])
        for first,second in zip(ring_ids,ring_ids[1:]):
            for j in range(segments):
                self.quad(first[j],first[j+1],second[j+1],second[j])
        self.mark_part(name,start)

    def tube(self,name,points,radius,color,weight=None,sides=7):
        """连续圆截面曲管用于触角、足和鳃盖线；仅封闭两端，不复制共面截面。"""
        start=len(self.positions); points=[Vector(p) for p in points]; rows=[]
        for i,p in enumerate(points):
            direction=(points[min(i+1,len(points)-1)]-points[max(i-1,0)]).normalized()
            reference=Vector((0,0,1)) if abs(direction.z)<0.9 else Vector((0,1,0))
            normal=direction.cross(reference).normalized()
            binormal=direction.cross(normal).normalized()
            r=radius*(1.0-0.28*i/max(1,len(points)-1))
            rows.append([self.vertex(p+r*(normal*math.cos(2*math.pi*j/sides)+binormal*math.sin(2*math.pi*j/sides)),
                 (j/sides,0.04),(color),0 if weight is None else weight(p)) for j in range(sides)])
        for a,b in zip(rows,rows[1:]):
            for j in range(sides):
                n=(j+1)%sides; self.quad(a[j],a[n],b[n],b[j])
        begin=self.vertex(points[0],(0.1,0.04),color,0 if weight is None else weight(points[0]))
        end=self.vertex(points[-1],(0.9,0.04),color,0 if weight is None else weight(points[-1]))
        for j in range(sides):
            n=(j+1)%sides; self.triangle(begin,rows[0][n],rows[0][j]); self.triangle(end,rows[-1][j],rows[-1][n])
        self.mark_part(name,start)

    def finish(self, material):
        mesh=bpy.data.meshes.new("SM_DBA_"+self.species+"_Geometry")
        mesh.from_pydata(self.positions,[],self.faces); mesh.update()
        obj=bpy.data.objects.new("SM_DBA_"+self.species,mesh)
        bpy.context.collection.objects.link(obj)
        uv=mesh.uv_layers.new(name="FaunaUV")
        colors=mesh.color_attributes.new(name=COLOR_LAYER,type="FLOAT_COLOR",domain="CORNER")
        for polygon in mesh.polygons:
            polygon.use_smooth=True
            for li in polygon.loop_indices:
                vi=mesh.loops[li].vertex_index
                uv.data[li].uv=self.uvs[vi]
                colors.data[li].color=self.colors[vi]
        mesh.color_attributes.active_color=colors
        mesh.materials.append(material)
        obj["Coordinates"]="X forward / Y lateral / Z up; source meters; UE converts to centimeters once"
        obj["MotionWeights"]=COLOR_LAYER+" RGB=part tint; A=wing or tail deformation"
        obj["ArtAcceptance"]="Pending human review"
        return obj

def butterfly_wing(builder,name,controls,center,side,zbase):
    """带稠密径向拓扑的单层翼片；铰链沿X，前后翼存在真实高度差。"""
    start=len(builder.positions); contour=catmull_closed(controls,5)
    center=np.asarray(center); rows=[]; rings=7
    def add(point):
        x,y=point
        arch=0.0012*math.sin(math.pi*np.clip((y-HINGE_Y_M)/0.065,0,1))
        p=(x,side*y,zbase+arch)
        uv=(np.clip(y/0.067,0.005,0.995),np.clip(0.15+(x+0.055)/0.105*0.78,0.105,0.98))
        return builder.vertex(p,uv,(0.72,0.84,1.0),motion_butterfly(p))
    middle=add(center)
    for ri in range(1,rings+1):
        rows.append([add(center+(p-center)*ri/rings) for p in contour])
    for j in range(len(contour)):
        n=(j+1)%len(contour)
        ids=(middle,rows[0][j],rows[0][n])
        if side<0: ids=ids[::-1]
        builder.triangle(*ids)
    for row,next_row in zip(rows,rows[1:]):
        for j in range(len(contour)):
            n=(j+1)%len(contour)
            ids=(row[j],next_row[j],next_row[n],row[n])
            if side<0: ids=ids[::-1]
            builder.quad(*ids)
    builder.mark_part(name,start)

def build_butterfly(material):
    builder=MeshBuilder("ButterflyAzure")
    for side,label in ((1,"Left"),(-1,"Right")):
        butterfly_wing(builder,label+"Forewing",FORE,(0.012,0.025),side,0.0018)
        butterfly_wing(builder,label+"Hindwing",HIND,(-0.025,0.024),side,0.0009)
    builder.ellipse("Abdomen",(-0.018,0,0.002),(0.021,0.0034,0.0036),(0.14,0.20,0.30),segments=18,rings=12)
    builder.ellipse("Thorax",(0.008,0,0.003),(0.012,0.0045,0.0052),(0.24,0.29,0.40),segments=20,rings=10)
    builder.ellipse("Head",(0.023,0,0.0035),(0.0054,0.0038,0.0042),(0.12,0.17,0.26),segments=18,rings=8)
    for side,label in ((1,"Left"),(-1,"Right")):
        builder.ellipse(label+"Eye",(0.024,side*0.00315,0.0044),(0.0020,0.0013,0.0020),(0.025,0.045,0.07),segments=12,rings=7)
        points=[]
        for t in np.linspace(0,1,15):
            points.append((0.026+0.020*t,side*(0.0018+0.0068*t+0.0028*t*t),0.004+0.005*math.sin(t*math.pi/2)))
        builder.tube(label+"Antenna",points,0.00024,(0.14,0.22,0.30),sides=6)
        builder.ellipse(label+"AntennaClub",points[-1],(0.0012,0.00052,0.00055),(0.32,0.41,0.54),segments=10,rings=6)
        for i,x in enumerate((0.014,0.007,-0.001)):
            builder.tube(label+"Leg"+str(i+1),[(x,side*0.003,-0.001),(x-0.002,side*0.006,-0.004),
                 (x-0.006,side*0.010,-0.006),(x-0.004,side*0.013,-0.007)],0.00019,(0.18,0.23,0.33),sides=5)
    return builder,builder.finish(material)

def fish_fin(builder,name,points,center,color=(0.62,0.79,0.96)):
    """单层弯曲鳍面，径向网格与UV匹配鳍条纹；没有透明矩形或盒状替代物。"""
    start=len(builder.positions); outline=catmull_closed(points,4); center=np.asarray(center)
    rows=[]; rings=5
    def add(p,radial,angle):
        return builder.vertex(p,(0.05+0.9*angle,0.018+0.065*radial),color,motion_fish(p))
    mid=add(center,0,0.5)
    for r in range(1,rings+1):
        rows.append([add(center+(p-center)*r/rings,r/rings,j/len(outline)) for j,p in enumerate(outline)])
    for j in range(len(outline)):
        n=(j+1)%len(outline); builder.triangle(mid,rows[0][j],rows[0][n])
    for a,b in zip(rows,rows[1:]):
        for j in range(len(outline)):
            n=(j+1)%len(outline); builder.quad(a[j],b[j],b[n],a[n])
    builder.mark_part(name,start)

def fish_radii(t):
    fullness=max(0,math.sin(math.pi*t))**0.82
    return 0.0035+0.0215*fullness*(0.90+0.1*t),0.0045+0.0305*fullness

def build_fish(material):
    builder=MeshBuilder("FishSilver"); start=0; rows=[]; rings=35; sides=28
    for i in range(rings+1):
        t=i/rings; x=-0.080+0.170*t; ry,rz=fish_radii(t); row=[]
        for j in range(sides+1):
            q=j/sides; theta=2*math.pi*q
            p=(x,ry*math.cos(theta),rz*math.sin(theta))
            dorsal=max(0,math.sin(theta))
            color=(0.89-0.40*dorsal,0.95-0.31*dorsal,1.0-0.23*dorsal)
            row.append(builder.vertex(p,(0.02+0.96*t,0.12+0.76*q),color,motion_fish(p)))
        rows.append(row)
    tail=builder.vertex((-0.081,0,0),(0.02,0.50),(0.70,0.82,0.94),motion_fish((-0.081,0,0)))
    nose=builder.vertex((0.094,0,-0.0006),(0.98,0.50),(0.78,0.88,0.95),0)
    for j in range(sides):
        builder.triangle(tail,rows[0][j+1],rows[0][j]); builder.triangle(nose,rows[-1][j],rows[-1][j+1])
    for a,b in zip(rows,rows[1:]):
        for j in range(sides): builder.quad(a[j],a[j+1],b[j+1],b[j])
    builder.mark_part("StreamlinedBody",start)
    fish_fin(builder,"ForkedCaudalFin",[(-0.079,0,-0.005),(-0.103,0,-0.019),(-0.141,0,-0.044),
        (-0.131,0,-0.020),(-0.116,0,0),(-0.131,0,0.020),(-0.141,0,0.044),
        (-0.103,0,0.019),(-0.079,0,0.005)],(-0.105,0,0))
    fish_fin(builder,"DorsalFin",[(0.028,0,0.025),(0.012,0,0.047),(-0.009,0,0.052),
        (-0.037,0,0.028),(-0.010,0,0.029)],(-0.006,0,0.034))
    fish_fin(builder,"AnalFin",[(0.000,0,-0.031),(-0.020,0,-0.048),(-0.040,0,-0.042),
        (-0.053,0,-0.024),(-0.028,0,-0.026)],(-0.027,0,-0.032))
    for side,label in ((1,"Left"),(-1,"Right")):
        fish_fin(builder,label+"PectoralFin",[(0.034,side*0.017,-0.006),(0.013,side*0.035,-0.014),
             (-0.007,side*0.040,-0.020),(0.003,side*0.022,-0.018),(0.027,side*0.017,-0.012)],
             (0.014,side*0.025,-0.013))
        fish_fin(builder,label+"PelvicFin",[(-0.007,side*0.009,-0.029),(-0.025,side*0.024,-0.045),
            (-0.041,side*0.020,-0.039),(-0.025,side*0.010,-0.026)],(-0.025,side*0.014,-0.032))
        builder.ellipse(label+"Iris",(0.058,side*0.0165,0.0118),(0.0061,0.0028,0.0061),
             (0.44,0.65,0.77),segments=18,rings=10)
        builder.ellipse(label+"Pupil",(0.059,side*0.0191,0.0118),(0.0035,0.00085,0.0038),
             (0.012,0.018,0.028),segments=16,rings=9)
        builder.ellipse(label+"EyeGlint",(0.0603,side*0.0198,0.0140),(0.00095,0.00032,0.00095),
             (0.90,0.98,1.0),segments=10,rings=6)
        gill=[]
        for t in np.linspace(0,1,12):
            z=-0.018+0.044*t; x=0.040-0.007*math.sin(math.pi*t)
            ry,rz=fish_radii((x+0.080)/0.170)
            y=side*(ry*math.sqrt(max(0,1-(z/rz)**2))+0.00022)
            gill.append((x,y,z))
        builder.tube(label+"GillCover",gill,0.00022,(0.20,0.34,0.43),motion_fish,sides=5)
    mouth=[(0.0922,0.0047*math.cos(t),-0.001+0.0022*math.sin(t)) for t in np.linspace(0,2*math.pi,19)]
    builder.tube("MouthRim",mouth,0.00019,(0.29,0.37,0.41),sides=5)
    return builder,builder.finish(material)

def write_png(path,rgb):
    """以确定的8位sRGB或数据编码写真实PNG，不依赖外部图像生成服务。"""
    data=np.clip(np.rint(rgb*255),0,255).astype(np.uint8)
    h,w,_=data.shape
    def chunk(kind,payload):
        return struct.pack(">I",len(payload))+kind+payload+struct.pack(">I",zlib.crc32(kind+payload)&0xffffffff)
    raw=b"".join(b"\0"+row.tobytes() for row in data)
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+
                     chunk(b"IDAT",zlib.compress(raw,8))+chunk(b"IEND",b""))

def polygon_distance(x,y,points):
    """计算纹理UV所映射翼轮廓的最近边距离，用于真实轮廓内的边缘色带。"""
    best=np.full(x.shape,10.0,dtype=np.float32)
    for a,b in zip(points,np.roll(points,-1,axis=0)):
        ab=b-a; t=np.clip(((x-a[0])*ab[0]+(y-a[1])*ab[1])/max(1e-12,float(np.dot(ab,ab))),0,1)
        distance=(x-a[0]-t*ab[0])**2+(y-a[1]-t*ab[1])**2
        np.minimum(best,distance,out=best)
    return np.sqrt(best)

def normal_from_height(height,strength):
    """微表面高度生成OpenGL切线法线；V向上，PNG行向下，因此反转行导数。"""
    dv,du=np.gradient(height.astype(np.float32))
    nx=-du*height.shape[1]*strength; ny=dv*height.shape[0]*strength
    normal=np.stack((nx,ny,np.ones_like(nx)),axis=-1)
    normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
    return normal*0.5+0.5

def texture_maps(species,size):
    """艺术方向明确的程序纹理：蝶翅边缘/翅脉/珠点，鱼鳞/侧线/背腹与鳍条。"""
    u,v=np.meshgrid(np.linspace(0,1,size,dtype=np.float32),np.linspace(1,0,size,dtype=np.float32))
    if species=="ButterflyAzure":
        x=(v-0.15)/0.78*0.105-0.055; y=u*0.067
        dist=np.minimum(polygon_distance(x,y,catmull_closed(FORE,4)),
                        polygon_distance(x,y,catmull_closed(HIND,4)))
        interior=smooth01((dist-0.0010)/0.0024)
        rootx=np.where(x>-0.005,0.010,-0.018)
        angle=np.arctan2(x-rootx,np.maximum(0.0001,y-0.002))
        veins=np.exp(-(np.sin(angle*10.5+0.4)/(0.055+0.02*u))**2)
        blue=np.stack((0.09+0.09*np.cos(v*7.0)**2,
                       0.30+0.22*np.sin(u*2.8)**2,
                       0.70+0.26*np.sin(v*3.4+0.5)**2),axis=-1)
        blue=blue*(1-0.70*veins[...,None])
        edge=np.asarray((0.018,0.027,0.07))
        base=edge+(blue-edge)*interior[...,None]
        pearl=np.zeros_like(u)
        for px,py in [(0.034,0.049),(0.024,0.056),(0.013,0.050),(0.002,0.040),
                      (-0.019,0.040),(-0.031,0.040),(-0.042,0.031),(-0.043,0.023)]:
            pearl=np.maximum(pearl,np.exp(-(((x-px)/0.00165)**2+((y-py)/0.0010)**2)*2))
        base=base*(1-pearl[...,None])+np.asarray((0.79,0.90,1.0))*pearl[...,None]
        scale=0.012*np.sin(u*980+np.sin(v*130)*1.5)*np.sin(v*1120)
        base+=scale[...,None]*interior[...,None]
        rough=np.clip(0.39+0.14*veins+0.05*(1-interior)-0.05*pearl,0.28,0.64)
        height=0.009*veins+0.003*np.sin(u*980)*np.sin(v*1120)
        body=v<0.105
        base[body]=np.stack((0.69+u[body]*0.1,0.77+u[body]*0.08,0.87+u[body]*0.06),axis=-1)
        rough[body]=0.49; height[body]=0.002*np.sin(u[body]*160)
        normal=normal_from_height(height,0.10)
    else:
        q=np.clip((v-0.12)/0.76,0,1); theta=q*2*math.pi
        dorsal=smooth01((np.sin(theta)+0.10)/1.1)
        belly=smooth01((-np.sin(theta)-0.10)/0.9)
        side=np.asarray((0.82,0.90,0.95))
        back=np.asarray((0.22,0.42,0.56))
        base=side[None,None,:]+(back-side)[None,None,:]*dorsal[...,None]
        base=base*(1-0.25*belly[...,None])+np.asarray((0.94,0.96,0.92))*0.25*belly[...,None]
        row=np.floor(q*22); sx=np.mod(u*40+np.mod(row,2)*0.5,1)-0.5; sy=np.mod(q*22,1)
        arc=np.sqrt((sx/0.60)**2+((sy-0.14)/0.98)**2)
        scale=np.exp(-((arc-0.75)/0.040)**2)
        base-=scale[...,None]*np.asarray((0.06,0.055,0.035))
        lateral=np.exp(-(np.sin(theta)/0.035)**2)*smooth01((u-0.08)/0.1)*(1-smooth01((u-0.80)/0.1))
        base-=lateral[...,None]*np.asarray((0.07,0.08,0.085))
        rough=np.clip(0.31+0.10*scale+0.05*dorsal,0.26,0.5)
        height=0.009*scale+0.0015*np.sin(u*450)*np.sin(q*320)
        fins=v<0.105
        rays=0.5+0.5*np.cos(u*30*2*math.pi)
        base[fins]=np.stack((0.63+0.20*rays[fins],0.78+0.13*rays[fins],0.94+0.04*rays[fins]),axis=-1)
        rough[fins]=0.41+0.06*rays[fins]; height[fins]=0.011*rays[fins]
        normal=normal_from_height(height,0.085)
    return {"BaseColor":np.clip(base,0,1),"Roughness":np.repeat(rough[...,None],3,axis=-1),"Normal":normal}

def material_for(species,paths):
    """Blender预览采用与UE预期一致的BaseColor×顶点RGB、粗糙度和切线法线。"""
    material=bpy.data.materials.new("M_Source_"+species); material.use_nodes=True
    nodes=material.node_tree.nodes; links=material.node_tree.links
    principled=nodes.get("Principled BSDF")
    principled.inputs["Metallic"].default_value=0.28 if species=="FishSilver" else 0.08
    principled.inputs["Roughness"].default_value=0.42
    color=nodes.new("ShaderNodeVertexColor"); color.layer_name=COLOR_LAYER
    multiply=nodes.new("ShaderNodeMixRGB"); multiply.blend_type="MULTIPLY"; multiply.inputs[0].default_value=1
    for kind,path in paths.items():
        image=bpy.data.images.load(str(path),check_existing=True)
        image.colorspace_settings.name="sRGB" if kind=="BaseColor" else "Non-Color"
        texture=nodes.new("ShaderNodeTexImage"); texture.image=image; texture.label=kind
        if kind=="BaseColor":
            links.new(texture.outputs["Color"],multiply.inputs[1]); links.new(color.outputs["Color"],multiply.inputs[2])
            links.new(multiply.outputs[0],principled.inputs["Base Color"])
        elif kind=="Roughness": links.new(texture.outputs["Color"],principled.inputs["Roughness"])
        else:
            normal=nodes.new("ShaderNodeNormalMap"); normal.inputs["Strength"].default_value=0.65
            links.new(texture.outputs["Color"],normal.inputs["Color"]); links.new(normal.outputs["Normal"],principled.inputs["Normal"])
    return material

def inspect_geometry(obj):
    """实际几何门禁：有限坐标、非退化三角面、无反向共面复制、UV和RGBA范围。"""
    mesh=obj.data; mesh.calc_loop_triangles()
    positions=np.asarray([obj.matrix_world@v.co for v in mesh.vertices],dtype=np.float64)
    triangles=np.asarray([t.vertices for t in mesh.loop_triangles],dtype=np.int32)
    area=np.linalg.norm(np.cross(positions[triangles[:,1]]-positions[triangles[:,0]],
                               positions[triangles[:,2]]-positions[triangles[:,0]]),axis=1)*0.5
    if not np.isfinite(positions).all() or np.any(area<1e-13):
        raise RuntimeError("几何非有限或出现退化三角形："+obj.name)
    keys=[tuple(sorted(tuple(np.round(positions[j],8)) for j in tri)) for tri in triangles]
    if len(keys)!=len(set(keys)): raise RuntimeError("存在共面重复/反向复制三角形："+obj.name)
    attribute=mesh.color_attributes.get(COLOR_LAYER) or mesh.color_attributes.active_color
    if attribute is None: raise RuntimeError("缺少顶点RGBA："+obj.name)
    colors=np.asarray([c.color for c in attribute.data],dtype=float)
    uv=np.asarray([tuple(v.uv) for v in mesh.uv_layers.active.data],dtype=float)
    if colors[:,3].min()>0.001 or colors[:,3].max()<0.99 or len(np.unique(np.round(colors[:,:3],3),axis=0))<4:
        raise RuntimeError("部位颜色或动画权重缺失："+obj.name)
    if uv.min()<-0.001 or uv.max()>1.001: raise RuntimeError("UV超出约定："+obj.name)
    # 不依赖FBX重排后的角点顺序；按线性RGBA的8位有效精度比较完整多重集合。
    rgba8=np.clip(np.rint(colors*255),0,255).astype(np.uint8)
    order=np.lexsort((rgba8[:,3],rgba8[:,2],rgba8[:,1],rgba8[:,0]))
    rgba_hash=hashlib.sha256(rgba8[order].tobytes()).hexdigest()
    return {"vertices":len(positions),"triangles":len(triangles),"minTriangleAreaM2":float(area.min()),
        "boundsMinM":positions.min(axis=0).tolist(),"boundsMaxM":positions.max(axis=0).tolist(),
        "dimensionsM":(positions.max(axis=0)-positions.min(axis=0)).tolist(),
        "maxExtentCm":float((positions.max(axis=0)-positions.min(axis=0)).max()*50),
        "vertexColorLayer":attribute.name,"alphaMin":float(colors[:,3].min()),"alphaMax":float(colors[:,3].max()),
        "colorVariants":len(np.unique(np.round(colors[:,:3],3),axis=0)),"rgba8BitMultisetSha256":rgba_hash,
        "zeroWeightCorners":int((colors[:,3]<0.001).sum()),"fullWeightCorners":int((colors[:,3]>0.99).sum()),
        "uvMin":uv.min(axis=0).tolist(),"uvMax":uv.max(axis=0).tolist(),"duplicateTriangles":0,"degenerateTriangles":0}

def export_and_verify(obj,path):
    """按本机已修正FBX合同导出后在独立临时场景重导入，验证单位/RGBA/三角数。"""
    for other in bpy.context.selected_objects: other.select_set(False)
    obj.select_set(True); bpy.context.view_layer.objects.active=obj
    original=inspect_geometry(obj)
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={"MESH"},
        axis_forward="X",axis_up="Z",apply_unit_scale=True,apply_scale_options="FBX_SCALE_UNITS",
        bake_anim=False,add_leaf_bones=False,use_mesh_modifiers=True,mesh_smooth_type="FACE",
        colors_type="LINEAR",use_tspace=True,path_mode="AUTO")
    source_scene=bpy.context.scene
    verify_scene=bpy.data.scenes.new("VERIFY_"+obj.name); verify_scene.unit_settings.system="METRIC"
    bpy.context.window.scene=verify_scene
    # Blender默认按sRGB解释FBX颜色；此源合同为LINEAR，必须显式匹配，避免伽马误差。
    bpy.ops.import_scene.fbx(filepath=str(path),use_custom_normals=True,colors_type="LINEAR")
    meshes=[o for o in verify_scene.objects if o.type=="MESH"]
    if len(meshes)!=1: raise RuntimeError("FBX重导入未得到恰好一个真实网格")
    observed=inspect_geometry(meshes[0])
    if observed["triangles"]!=original["triangles"]:
        raise RuntimeError("FBX往返三角形数量变化")
    if not np.allclose(observed["dimensionsM"],original["dimensionsM"],rtol=0.002,atol=0.00001):
        raise RuntimeError("FBX米单位往返尺寸不匹配："+repr(observed))
    if abs(observed["alphaMax"]-original["alphaMax"])>0.005 or observed["alphaMin"]>0.005:
        raise RuntimeError("FBX往返翼/尾权重丢失")
    if observed["rgba8BitMultisetSha256"]!=original["rgba8BitMultisetSha256"]:
        raise RuntimeError("FBX往返完整线性RGBA集合不一致，不能只以Alpha范围通过")
    bpy.context.window.scene=source_scene
    for imported in list(verify_scene.objects): bpy.data.objects.remove(imported,do_unlink=True)
    bpy.data.scenes.remove(verify_scene)
    return original,observed

def render_preview(scene,obj,path,view):
    """真实Cycles渲染，使用本后台进程四线程；相机/灯只存在源预览场景，FBX不含它们。"""
    bpy.context.window.scene=scene
    scene.render.engine="CYCLES"; scene.cycles.samples=32; scene.cycles.use_denoising=True
    scene.render.resolution_x=1200; scene.render.resolution_y=900; scene.render.resolution_percentage=100
    scene.render.threads_mode="FIXED"; scene.render.threads=4
    scene.render.image_settings.file_format="PNG"; scene.render.image_settings.color_mode="RGBA"
    scene.view_settings.view_transform="AgX"; scene.render.film_transparent=False
    world=bpy.data.worlds.new("Studio_"+scene.name); world.use_nodes=True
    world.node_tree.nodes["Background"].inputs[0].default_value=(0.065,0.085,0.12,1)
    world.node_tree.nodes["Background"].inputs[1].default_value=0.35; scene.world=world
    butterfly="Butterfly" in obj.name
    camera_data=bpy.data.cameras.new("ReviewCamera_"+view); camera=bpy.data.objects.new(camera_data.name,camera_data)
    scene.collection.objects.link(camera)
    if butterfly:
        camera.location=(0,-0.0001,0.35) if view=="Top" else (0.14,-0.17,0.27)
        target=Vector((0,0,0)); camera_data.ortho_scale=0.172
    else:
        camera.location=(0.045,-0.40,0.045) if view=="Side" else (0.21,-0.30,0.16)
        target=Vector((-0.015,0,0)); camera_data.ortho_scale=0.30
    camera.rotation_euler=(target-camera.location).to_track_quat("-Z","Y").to_euler()
    camera_data.type="ORTHO"; scene.camera=camera
    lights=[]
    # 米制小模型的灯距仅十几厘米，使用亚瓦特照明，避免近距能量导致蓝紫色过曝。
    for name,location,energy,size in [
        ("Key",(-0.10,-0.16,0.30),0.70,0.24),("Fill",(0.20,0.10,0.14),0.35,0.20),("Rim",(-0.19,0.16,0.20),0.80,0.18)]:
        data=bpy.data.lights.new(name+"_"+view,"AREA"); data.energy=energy; data.shape="DISK"; data.size=size
        light=bpy.data.objects.new(data.name,data); scene.collection.objects.link(light); light.location=location
        light.rotation_euler=(target-light.location).to_track_quat("-Z","Y").to_euler(); lights.append(light)
    scene.render.filepath=str(path)
    bpy.ops.render.render(write_still=True)
    for transient in lights+[camera]: bpy.data.objects.remove(transient,do_unlink=True)

def main():
    """有界制作主入口；失败直接抛错，保留真实异常，不把不完整产物记为通过。"""
    if not bpy.app.background: raise RuntimeError("必须使用自己拥有的 --background Blender 进程")
    if bpy.app.version[:2]!=(5,2): raise RuntimeError("必须使用已核对的Blender5.2")
    args=sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else []
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--texture-size",type=int,choices=(512,1024),default=1024)
    parser.add_argument("--render",action="store_true")
    options=parser.parse_args(args)
    old=OUT/"FaunaSourceManifest.json"
    if old.exists() and json.loads(old.read_text(encoding="utf-8")).get("generator")!=GENERATOR:
        raise RuntimeError("目标清单属于其他作者，拒绝覆盖")
    for folder in (OUT,OUT/"Meshes",OUT/"Textures",OUT/"Previews"): folder.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    # 只重建本工具可复现产物，不生成未登记的 .blend1 自动备份。
    bpy.context.preferences.filepaths.save_version=0
    scenes=[]; models=[]; rows=[]; files=[]
    for species,builder_fn in (("ButterflyAzure",build_butterfly),("FishSilver",build_fish)):
        scene=bpy.context.scene if not scenes else bpy.data.scenes.new("Source_"+species)
        scene.name="Source_"+species; bpy.context.window.scene=scene
        scene.unit_settings.system="METRIC"; scene.unit_settings.scale_length=1.0
        maps=texture_maps(species,options.texture_size); paths={}; texture_info={}
        for kind,pixels in maps.items():
            path=OUT/"Textures"/("T_DBA_"+species+"_"+kind+"_Source.png")
            write_png(path,pixels); paths[kind]=path; files.append(path)
            texture_info[kind]={"path":str(path.relative_to(OUT)).replace("\\","/"),
                "size":[options.texture_size,options.texture_size],"srgb":kind=="BaseColor",
                "channelStdDev":[float(x) for x in pixels.reshape(-1,3).std(axis=0)]}
        material=material_for(species,paths); builder,obj=builder_fn(material)
        path=OUT/"Meshes"/("SM_DBA_"+species+".fbx")
        geometry,roundtrip=export_and_verify(obj,path); files.append(path)
        if species=="ButterflyAzure":
            if not 0.10<geometry["dimensionsM"][1]<0.15: raise RuntimeError("蝴蝶翼展不在授权尺寸范围")
        elif not 0.22<geometry["dimensionsM"][0]<0.26: raise RuntimeError("鱼总长不在授权尺寸范围")
        row={"species":species,"object":obj.name,"mesh":str(path.relative_to(OUT)).replace("\\","/"),
             "geometry":geometry,"fbxRoundtrip":roundtrip,"parts":builder.parts,"textures":texture_info,
             "technicalValidation":"passed","humanArtAcceptance":"pending"}
        if species=="ButterflyAzure":
            row["motionContract"]={"hingeYM":HINGE_Y_M,"hingeZM":HINGE_Z_M,
                "forewingBaseZM":0.0018,"hindwingBaseZM":0.0009,"maxCamberM":0.0012,
                "defaultWingAngleDegrees":0,"alpha":"smoothstep(0.0025,0.018,abs(Y)) on wings; zero on body/antennae/legs",
                "uv":"Wing U=abs(Y)/0.067; V=0.15+(X+0.055)/0.105*0.78. Body V=0.025..0.07."}
        else:
            row["motionContract"]={"tailDirection":"negative X","swimDisplacementAxis":"local Y",
                "alpha":"smoothstep(0,1,(0.035-X)/0.155); eyes/head X>=0.035 have zero weight",
                "uv":"Body U=0.02+0.96*((X+0.080)/0.170); V=0.12+0.76*q, q=atan2(Z/rz,Y/ry)/(2*pi). Fins V=0.018..0.083."}
        rows.append(row); scenes.append(scene); models.append(obj)
        print("FAUNA_FBX_VERIFIED",json.dumps({"species":species,"geometry":geometry},ensure_ascii=False),flush=True)
    if options.render:
        for scene,obj in zip(scenes,models):
            for view in (("Top","Oblique") if "Butterfly" in obj.name else ("Side","Oblique")):
                path=OUT/"Previews"/(obj.name.replace("SM_","")+"_"+view+".png")
                render_preview(scene,obj,path,view); files.append(path)
                print("FAUNA_REAL_RENDER_SAVED",str(path),flush=True)
    bpy.context.window.scene=scenes[0]
    bpy.ops.file.pack_all()
    blend=OUT/"DBA_Fauna_Models.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend),compress=True); files.append(blend)
    report={"schemaVersion":1,"generator":GENERATOR,
        "generatorSha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "blenderVersion":bpy.app.version_string,"sourceUnits":"meters","targetUnits":"centimeters",
        "axes":{"forward":"+X","lateral":"Y","up":"+Z"},"vertexColorLayer":COLOR_LAYER,
        "fbx":{"axisForward":"X","axisUp":"Z","applyUnitScale":True,"applyScaleOptions":"FBX_SCALE_UNITS","colorsType":"LINEAR"},
        "ueImport":{"factory":"FbxFactory","convertScene":True,"convertSceneUnit":True,"importUniformScale":1.0,
            "vertexColorImportOption":"REPLACE","normalImportMethod":"IMPORT_NORMALS","autoGenerateCollision":False,
            "processConsoleVariable":"Interchange.FeatureFlags.Import.FBX 0"},
        "normalConvention":"OpenGL tangent normal (+Y); UE import should enable FlipGreenChannel",
        "materialContract":"Two-sided; BaseColorTexture * VertexColor.rgb; VertexColor.a=motion weight; no opacity cutout",
        "technicalValidation":"passed","humanArtAcceptance":"pending","species":rows,
        "files":[{"path":str(f.relative_to(OUT)).replace("\\","/"),"bytes":f.stat().st_size,
                  "sha256":hashlib.sha256(f.read_bytes()).hexdigest()} for f in files]}
    (OUT/"FaunaSourceManifest.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("FAUNA_SOURCE_COMPLETE",json.dumps({"species":len(rows),"files":len(files),"technicalValidation":"passed",
          "humanArtAcceptance":"pending","manifest":str(OUT/"FaunaSourceManifest.json")},ensure_ascii=False),flush=True)

if __name__=="__main__":
    main()
