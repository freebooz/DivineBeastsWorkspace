# -*- coding: utf-8 -*-
"""以用户参考图制作项目层前端神兽圣境，必须由 Blender 5.2 执行。

输出真实可编辑 BLEND、分组 FBX、PBR 源纹理与制作清单。坐标单位为米，
中央角色脚底是 (0,0,0)，背景沿 +Y；导入 UE 后使用厘米并显式转换轴。
不读取账号、不实现玩法权威、不创建 UE 二进制资产。随机种子固定用于
重复制作；用户手改的输出必须先另存，脚本以新 --output 目录避免覆盖。
"""
import argparse
import json
import math
import random
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


random.seed(20261011)
TAU = math.tau
MATERIALS = []
BATCHES = {}
OUT = None


class GeometryBatch:
    """按美术职责收集真实顶点/面，减少导出与 UE Actor 数量。

    所有几何保持全局米制坐标；UV 是世界投影或每个制作原语的局部展开，
    不把外观当成一张背景面。材质槽按统一索引固定，重跑不会随机换色。
    """
    def __init__(self, name):
        self.name = name
        self.vertices = []
        self.faces = []
        self.materials = []
        self.uvs = []

    def add(self, vertices, faces, material, uvs=None):
        """追加局部几何；材质参数为本生成器拥有的真实 Blender 材质。"""
        offset = len(self.vertices)
        self.vertices.extend(vertices)
        index = MATERIALS.index(material)
        self.faces.extend(tuple(offset + i for i in face) for face in faces)
        self.materials.extend([index] * len(faces))
        self.uvs.extend(uvs if uvs else [(v[0] / 3, v[1] / 3) for v in vertices])

    def object(self):
        """生成可独立编辑的 Mesh，对每个环面逐顶点填写真实 UV。"""
        mesh = bpy.data.meshes.new(self.name)
        mesh.from_pydata(self.vertices, [], self.faces)
        mesh.update()
        for material in MATERIALS:
            mesh.materials.append(material)
        uv = mesh.uv_layers.new(name='UVMap')
        for polygon, index in zip(mesh.polygons, self.materials):
            polygon.material_index = index
            polygon.use_smooth = self.name not in ('Architecture', 'ZodiacInlays')
            for loop_id in polygon.loop_indices:
                uv.data[loop_id].uv = self.uvs[mesh.loops[loop_id].vertex_index]
        obj = bpy.data.objects.new('SM_DBA_Sanctuary_' + self.name, mesh)
        bpy.context.collection.objects.link(obj)
        obj['purpose_zh'] = '神兽联盟角色选择与创建三维圣境的' + self.name
        return obj


def batch(name):
    """取得当前制作分组，不持有 Blender 或 UE 运行态业务状态。"""
    if name not in BATCHES:
        BATCHES[name] = GeometryBatch(name)
    return BATCHES[name]


def texture(name, color_a, color_b, kind='stone', size=1024):
    """产生原创可导入的 PBR 基色纹理，浮点噪声在 CPU 上确定性计算。

    图像由 Blender 原生保存为 PNG；纹理不含用户原图投影或伪造三维内容。
    石纹包含细颗粒、粗色斑与裂缝，树皮包含纵向纤维，叶片包含叶脉。
    """
    y, x = np.mgrid[0:size, 0:size].astype(np.float32) / size
    n = np.zeros_like(x)
    for freq, amount in ((4, .24), (13, .12), (37, .07), (113, .035)):
        phase = random.random() * TAU
        n += amount * np.sin(x * TAU * freq + phase + 2*np.sin(y*freq*2)) * np.cos(y*TAU*freq*.73)
    n = np.clip(.52 + n + np.random.default_rng(42).normal(0,.03,x.shape), 0, 1)
    if kind == 'bark':
        n = np.clip(n - .25 * (np.sin(x*TAU*37 + .7*np.sin(y*12)) > .73),0,1)
    if kind == 'leaf':
        n = np.clip(.45+.3*np.sin(x*math.pi)+.1*np.sin(y*55),0,1)
    a, b = np.asarray(color_a), np.asarray(color_b)
    rgb = a[None,None,:] * (1-n[:,:,None]) + b[None,None,:] * n[:,:,None]
    if kind == 'stone':
        crack = np.minimum(np.abs(np.sin(x*TAU*3+np.sin(y*TAU*4))),np.abs(np.sin(y*TAU*3+np.sin(x*TAU*2)))) < .018
        rgb[crack] *= .56
    rgba = np.concatenate((rgb.astype(np.float32), np.ones((size,size,1),np.float32)),axis=2)
    image = bpy.data.images.new(name, width=size,height=size)
    image.colorspace_settings.name = 'sRGB'
    image.pixels.foreach_set(rgba.ravel())
    image.filepath_raw = str(OUT / 'Textures' / (name+'.png'))
    image.file_format = 'PNG'
    image.save()
    image.pack()
    return image


def material(name, color, roughness=.7, metallic=0, image=None, emission=0):
    """创建 Principled PBR；英文稳定名称用于 FBX 与 UE 材质映射。"""
    mat = bpy.data.materials.new('M_DBA_Sanctuary_'+name)
    mat.use_nodes = True
    mat.diffuse_color = (*color,1)
    shader = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value = (*color,1)
    shader.inputs['Roughness'].default_value = roughness
    shader.inputs['Metallic'].default_value = metallic
    if image:
        node = mat.node_tree.nodes.new('ShaderNodeTexImage')
        node.image = image
        mat.node_tree.links.new(node.outputs['Color'],shader.inputs['Base Color'])
        # 真实纹理驱动细微凹凸；UE 导入脚本重建对应纹理材质，不依赖 FBX 携带节点。
        bump = mat.node_tree.nodes.new('ShaderNodeBump')
        bump.inputs['Strength'].default_value = .18
        bump.inputs['Distance'].default_value = .035
        mat.node_tree.links.new(node.outputs['Color'], bump.inputs['Height'])
        mat.node_tree.links.new(bump.outputs['Normal'], shader.inputs['Normal'])
    if emission:
        shader.inputs['Emission Color'].default_value=(*color,1)
        shader.inputs['Emission Strength'].default_value=emission
    MATERIALS.append(mat)
    return mat


def box(group, center, extent, mat, angle=0):
    """米制完整立方体，extent 是半尺寸；支持台阶/瓦片绕 Z 旋转。"""
    c,s=math.cos(angle),math.sin(angle)
    verts=[]
    for x,y,z in ((-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)):
        dx,dy=x*extent[0],y*extent[1]
        verts.append((center[0]+c*dx-s*dy,center[1]+s*dx+c*dy,center[2]+z*extent[2]))
    batch(group).add(verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],mat)


def ellipsoid(group, center, radii, mat, segments=24, rings=14, angle=0):
    """可编辑椭球曲面用于雕刻体块、岩石、莲花芽；表面含真实法线。"""
    vertices=[]
    c,s=math.cos(angle),math.sin(angle)
    for j in range(rings+1):
        phi=math.pi*j/rings
        for i in range(segments):
            theta=TAU*i/segments
            x=radii[0]*math.sin(phi)*math.cos(theta)
            y=radii[1]*math.sin(phi)*math.sin(theta)
            vertices.append((center[0]+c*x-s*y,center[1]+s*x+c*y,center[2]+radii[2]*math.cos(phi)))
    faces=[]
    for j in range(rings):
        for i in range(segments):
            faces.append((j*segments+i,j*segments+(i+1)%segments,(j+1)*segments+(i+1)%segments,(j+1)*segments+i))
    batch(group).add(vertices,faces,mat,[(i/segments,j/rings) for j in range(rings+1) for i in range(segments)])


def tube(group, points, radius, mat, sides=10, taper=.35):
    """沿折线生成封口管状网格，用于藤蔓、雕刻线、角、枝干与兽体。

    radius 支持每点半径。切线与局部横截面逐点计算，垂直段也不会退化。
    """
    verts=[]
    n=len(points)
    for j,point in enumerate(points):
        p=Vector(point)
        prev=Vector(points[max(0,j-1)])
        nxt=Vector(points[min(n-1,j+1)])
        tangent=(nxt-prev).normalized()
        ref=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))
        a=tangent.cross(ref).normalized()
        b=tangent.cross(a).normalized()
        r=radius[j] if isinstance(radius,list) else radius*(1-(1-taper)*j/(n-1))
        for i in range(sides):
            v=p+r*(a*math.cos(TAU*i/sides)+b*math.sin(TAU*i/sides))
            verts.append(tuple(v))
    faces=[tuple(range(sides-1,-1,-1))]
    for j in range(n-1):
        for i in range(sides):
            faces.append((j*sides+i,j*sides+(i+1)%sides,(j+1)*sides+(i+1)%sides,(j+1)*sides+i))
    faces.append(tuple((n-1)*sides+i for i in range(sides)))
    batch(group).add(verts,faces,mat,[(i/sides,j*.3) for j in range(n) for i in range(sides)])


def ring(group, center, inner, outer, depth, mat, count=96, a0=0,a1=TAU,vertical=False):
    """实体环/弧段，月门是真实通孔，圆台饰带有厚度。"""
    vertices=[]
    for z in (-depth/2,depth/2):
        for r in (inner,outer):
            for i in range(count+1):
                a=a0+(a1-a0)*i/count
                p=(r*math.cos(a),r*math.sin(a),z)
                if vertical:p=(p[0],p[2],p[1])
                vertices.append(tuple(center[k]+p[k] for k in range(3)))
    n=count+1
    faces=[]
    for i in range(count):
        for u,v in ((0,1),(1,3),(3,2),(2,0)):
            faces.append((u*n+i,v*n+i,v*n+i+1,u*n+i+1))
    faces.extend([(0,n,3*n,2*n),(n-1,2*n-1,4*n-1,3*n-1)])
    batch(group).add(vertices,faces,mat)


def leaf(group, origin, end, width, mat, droop=.1):
    """具中央折脊与自然弯曲的双侧叶片/羽片；导出真实几何而非透明贴片。"""
    p,q=Vector(origin),Vector(end)
    direction=q-p
    side=direction.cross(Vector((0,0,1))).normalized()
    if side.length<.1:side=Vector((1,0,0))
    vertices=[]
    for j in range(5):
        t=j/4
        c=p+direction*t
        c.z+=math.sin(t*math.pi)*droop
        w=width*math.sin(t*math.pi)**.75
        vertices.extend((tuple(c-side*w),tuple(c+Vector((0,0,.035*math.sin(t*math.pi)))),tuple(c+side*w)))
    faces=[]
    for j in range(4):
        for i in range(2):faces.append((j*3+i,(j+1)*3+i,(j+1)*3+i+1,j*3+i+1))
    batch(group).add(vertices,faces,mat,[(i/2,j/4) for j in range(5) for i in range(3)])


def rock(center, size, mat, group='Terraces', seed=None):
    """原创非规则岩石；顶点扰动打破规则椭球，台地保持合理层次。"""
    rng=random.Random(seed or random.randrange(999999))
    seg,rings=12,8
    verts=[]
    for j in range(rings+1):
        phi=math.pi*j/rings
        for i in range(seg):
            th=TAU*i/seg
            noise=rng.uniform(.84,1.16)
            verts.append((center[0]+size[0]*math.sin(phi)*math.cos(th)*noise,center[1]+size[1]*math.sin(phi)*math.sin(th)*noise,center[2]+size[2]*math.cos(phi)*noise))
    faces=[(j*seg+i,j*seg+(i+1)%seg,(j+1)*seg+(i+1)%seg,(j+1)*seg+i) for j in range(rings) for i in range(seg)]
    batch(group).add(verts,faces,mat)


def lantern(x,y,z,scale=1):
    """完整石灯：基座、柱身、透光灯格、四角翘檐和宝珠。"""
    for h,w,d in ((.1,.48,.2),(.4,.33,.4),(.95,.22,.65),(1.45,.36,.18)):
        box('Architecture',(x,y,z+h*scale),(w*scale,w*scale,d*scale/2),STONE)
    box('Architecture',(x,y,z+1.25*scale),(.22*scale,.22*scale,.28*scale),GLOW)
    for a in range(4):
        th=TAU*a/4
        box('Architecture',(x+.28*scale*math.cos(th),y+.28*scale*math.sin(th),z+1.25*scale),(.035*scale,.035*scale,.32*scale),JADE)
    for k in range(5):
        w=(.58-k*.09)*scale
        box('Architecture',(x,y,z+(1.58+k*.065)*scale),(w,w,.045*scale),STONE)
    ellipsoid('Architecture',(x,y,z+1.98*scale),(.1*scale,.1*scale,.12*scale),GOLD,16,8)


def platform():
    """中央十二生肖圆台；脚底高度 0，周边水面 -0.55m。"""
    for inner,outer,z,d,mat in ((0,7,-.28,.48,STONE),(0,6.9,-.02,.08,STONE),(0,2.25,.025,.045,JADE),(2.26,2.36,.03,.05,GOLD),(3.4,3.47,.015,.04,GOLD),(5.8,5.92,.02,.05,GOLD),(6.85,7,.02,.1,JADE)):
        ring('Architecture',(0,0,z),inner,outer,d,mat)
    for j in range(12):
        a=TAU*j/12
        ring('ZodiacInlays',(0,0,.031),5.95,6.74,.025,JADE,10,a+.025,a+TAU/12-.025)
        cx,cy=6.3*math.cos(a+math.pi/12),6.3*math.sin(a+math.pi/12)
        # 十二生肖以真正石刻动物轮廓区分，未加入已取消的派系/五行规则。
        glyph(j,(cx,cy,.065),a+math.pi/12)
    for radius in (2.45,3.55,4.55,5.75):
        ring('ZodiacInlays',(0,0,.01),radius,radius+.028,.022,JADE)
    for j in range(40):
        angle=TAU*j/40
        points=[(r*math.cos(angle+.018*math.sin(r*2)),r*math.sin(angle+.018*math.sin(r*2)),.009) for r in np.linspace(2.4,5.78,10)]
        tube('ZodiacInlays',points,.012,JADE,5,1)
    # 中央云纹双鱼盘，角色站立不会被高凸起挡住。
    for sign in (-1,1):
        points=[]
        for i in range(80):
            a=i/79*TAU*1.6
            r=.12+1.1*i/79
            points.append((sign*(.35+r*math.cos(a)),sign*r*math.sin(a),.058))
        tube('ZodiacInlays',points,.028,GOLD,6,1)
    # 台阶和桥后沿 +Y 指向月门，中央持续留空用于真实角色。
    for j in range(3):
        box('Architecture',(0,7.5+j*.65,-.36+j*.12),(2.25,.5,.1),STONE)
    for j in range(28):
        box('Architecture',(0,11+j*.57,.08+j*.22),(2.3,.31,.12),STONE)
        for side in (-1,1):
            box('Architecture',(side*2.55,11+j*.57,.25+j*.22),(.23,.31,.18),JADE)
    for side in (-1,1):
        tube('Architecture',[(side*2.55,10.5,.4),(side*2.55,27.4,6.95)],.16,STONE,12,1)
        for y,z in ((-4.6,0),(4.3,0),(10,.12),(17,2.65),(25,5.75)):
            x=side*(4.8 if y<9 else 3)
            # 灯柱必须有实体承托；阶梯外侧石墩一直延伸到水下。
            if y>=9:box('Architecture',(x,y,(z-.8)/2),(.65,.7,(z+.8)/2),STONE)
            lantern(x,y,z,1 if y<9 else .75)
    box('Architecture',(0,9.5,-.06),(2.3,1.8,.16),STONE)
    ring('Architecture',(0,27.7,6.25),0,4.6,.45,STONE)
    for side in (-1,1):
        for j in range(9):
            box('Architecture',(side*(8+j*.65),6.3,-.3),(1.5,.55,.35),STONE)
    # 前景石板路具有可编辑的接缝与破损石块。
    for y in np.arange(-12,-6,.8):
        for x in np.arange(-7,7,1.2):
            if x*x+y*y<48:continue
            box('Architecture',(float(x),float(y),-.34+random.uniform(-.02,.02)),(.57,.37,.08),STONE,random.uniform(-.025,.025))


def glyph(index, center, angle):
    """用低浮雕线描区分生肖，线条是导出网格而非字符字体依赖。"""
    cx,cy,z=center
    def p(x,y):
        return(cx+(x*math.cos(angle)-y*math.sin(angle))*.65,cy+(x*math.sin(angle)+y*math.cos(angle))*.65,z)
    forms=[
        [(-.45,0),(-.1,.24),(.25,.22),(.45,.04),(.24,-.08),(-.15,-.12),(-.45,0)],
        [(-.45,.15),(.25,.22),(.45,.5),(.44,-.03),(-.15,-.2),(-.45,.15)],
        [(-.5,0),(-.3,.28),(.2,.3),(.48,.08),(.3,-.1),(-.35,-.13),(-.5,0)],
        [(-.38,-.12),(-.23,.28),(.02,.45),(.1,.75),(.2,.38),(.45,.1),(.26,-.14),(-.38,-.12)],
        [(-.5,-.18),(-.22,.14),(.08,-.13),(.28,.3),(.48,.45),(.36,.6)],
        [(-.45,-.18),(-.22,-.02),(.15,.32),(.42,.42),(.3,.1),(0,-.15),(-.36,.23)],
        [(-.42,-.1),(-.22,.21),(.18,.24),(.24,.55),(.45,.4),(.42,.18),(.2,-.1)],
        [(-.4,-.12),(-.2,.2),(.25,.22),(.5,.47),(.25,.38),(.45,.7),(.4,.15),(.15,-.14)],
        [(-.4,.04),(-.15,.35),(.17,.34),(.3,.1),(.18,-.2),(-.04,-.3),(-.28,-.02),(-.4,.04)],
        [(-.38,-.1),(-.24,.26),(.02,.25),(.28,.6),(.35,.2),(.05,-.15),(-.38,-.1)],
        [(-.5,0),(-.25,.27),(.18,.22),(.3,.5),(.48,.28),(.38,.04),(.1,-.12),(-.5,0)],
        [(-.4,0),(-.2,.27),(.28,.2),(.42,.04),(.25,-.15),(-.2,-.16),(-.4,0)],
    ]
    tube('ZodiacInlays',[p(*v) for v in forms[index]],.021,GOLD,6,1)
    if index not in (3,4,5,8,9):
        for x in (-.2,.19):tube('ZodiacInlays',[p(x,-.02),p(x-.03,-.37)],.017,GOLD,6,1)
    if index in (0,2,8,10,11):tube('ZodiacInlays',[p(-.4,0),p(-.65,.1),p(-.65,.32)],.018,GOLD,6,1)


def moon_gate():
    """月门、雕刻饰带与千年古树，真实完整圆形孔洞与分枝。"""
    ring('Architecture',(0,29,12.7),5.15,6.2,.7,STONE,160,vertical=True)
    for r in (5.18,5.4,5.94,6.15):
        ring('ZodiacInlays',(0,28.55,12.7),r,r+.065,.045,GOLD,160,vertical=True)
    for j in range(48):
        a=TAU*j/48
        points=[]
        for k in range(14):
            t=k/13*TAU
            rr=5.7+.15*math.sin(t)
            aa=a+.025*math.cos(t)
            points.append((rr*math.cos(aa),28.51,12.7+rr*math.sin(aa)))
        tube('ZodiacInlays',points,.042,JADE,6,1)
    # 树主干从左侧攀住月门，冠幅横跨顶部，保留中央远景通道。
    main=[(-6.6,27.5,5.8),(-6.8,27.9,8),(-6.4,28,11),(-5.9,28,14),(-4.5,29,17),(-1,29,19),(5,31,19.7),(10,32,20.5)]
    tube('Trees',main,[1.7,1.5,1.22,1.05,.9,.68,.42,.13],BARK,18)
    for j in range(9):
        angle=j*TAU/9
        root=[(-6.6,27.5,6.5),(-6.6+math.cos(angle)*2,27.5+math.sin(angle)*2,6),(-6.6+math.cos(angle)*4,27.5+math.sin(angle)*4,5.2)]
        tube('Trees',root,[.62,.32,.06],BARK,12)
    for j in range(13):
        t=j/12
        point=(-4+14*t,29+3*t,18.5+2*t)
        end=(point[0]+random.uniform(-5,5),point[1]+random.uniform(-4,4),point[2]+random.uniform(1.5,4.5))
        tube('Trees',[point,((point[0]+end[0])/2,end[1],end[2]-.7),end],[.42,.23,.035],BARK,12)
        crown(end,2.6,520)
    for j in range(15):
        a=j/14*math.pi
        points=[(-6.2+math.cos(a+k*.12)*1.8,27.5+math.sin(a+k*.12)*1.8,6+k*.68) for k in range(15)]
        tube('Vines',points,.08,MOSS,8,.8)
    for j in range(32):
        x=random.uniform(-8,11)
        y=random.uniform(27,32)
        z=random.uniform(19,21)
        length=random.uniform(1.2,5)
        points=[(x+math.sin(k*.7)*.12,y,z-length*k/8) for k in range(9)]
        tube('Vines',points,.025,BARK,6,.35)
        for k in range(2,9):
            p=points[k]
            leaf('Foliage',p,(p[0]+.38,p[1]-.1,p[2]-.1),.12,LEAF,.05)


def crown(center,radius,count):
    """用大量真实叶片构成树冠，避免用圆球代替树叶。"""
    for j in range(count):
        phi=random.uniform(0,TAU)
        r=radius*random.random()**.5
        x=center[0]+r*math.cos(phi)
        y=center[1]+r*math.sin(phi)
        z=center[2]+random.uniform(-.6,.6)+(1-r/radius)*.5
        length=random.uniform(.32,.65)
        a=random.uniform(0,TAU)
        leaf('Foliage',(x,y,z),(x+math.cos(a)*length,y+math.sin(a)*length,z+random.uniform(-.18,.18)),length*.22,random.choice((LEAF,LEAF,LEAFLIGHT)),.08)


def tree(center,height=6,radius=2.4):
    """多层树干与枝条形成自然林木，树冠和树皮分组导出。"""
    x,y,z=center
    tip=(x+random.uniform(-.8,.8),y+random.uniform(-.8,.8),z+height)
    tube('Trees',[(x,y,z),(x-.1,y,z+height*.4),tip],[height*.065,height*.05,.035],BARK,10)
    for j in range(5):
        a=TAU*j/5+random.uniform(-.2,.2)
        end=(tip[0]+radius*math.cos(a),tip[1]+radius*math.sin(a),tip[2]+random.uniform(-.4,.4))
        tube('Trees',[(x,y,z+height*.6),end],[height*.035,.025],BARK,8)
        crown(end,radius*.78,260)


def terrain():
    """水池两侧岩台、远景层叠悬峰及瀑布；中央视线保持通畅。"""
    terraces=[(-10,7,2.2,5,5,3), (13,6,3,5.5,5,3.5), (-13,20,7,5,5,6), (10,17,6,5,5,5), (-6,24,10,5,5,8), (16,29,13,6,6,10), (-22,28,13,5,6,11), (24,33,17,6,6,14)]
    for j,(x,y,top,sx,sy,sz) in enumerate(terraces):
        # 岩体最高点留在平台以下，避免穿过雕塑；浮雕台面承担实际承托。
        rock((x,y,top-sz*1.16-.08),(sx,sy,sz),STONE,seed=j+8)
        box('Architecture',(x,y,top-.23),(sx*.62,sy*.6,.23),STONE)
        for k in range(20):
            rx=x+random.uniform(-sx,sx)
            ry=y+random.uniform(-sy,sy)
            if (rx-x)**2+(ry-y)**2<8:continue
            rz=top+random.uniform(-.4,.1)
            rock((rx,ry,rz),(random.uniform(.4,1.1),random.uniform(.4,1.1),random.uniform(.2,.65)),MOSS,seed=j*100+k+50)
        for k in range(3):
            tree((x+random.uniform(-sx*.65,sx*.65),y+sy*.65,top-.2),random.uniform(2.8,5.2),random.uniform(1.3,2))
    for side in (-1,1):
        for j in range(13):
            x=side*random.uniform(7.8,18)
            y=random.uniform(-10,10)
            rock((x,y,-.35),(random.uniform(1,2.5),random.uniform(.9,2),random.uniform(.3,.9)),STONE)
            fern((x,y,.1),random.uniform(.6,1.5))
    for j in range(14):
        x=(-1 if j%2 else 1)*random.uniform(13,45)
        y=random.uniform(40,70)
        h=random.uniform(15,33)
        for k in range(3):
            rock((x+random.uniform(-2,2),y,k*h*.3),(random.uniform(3,5)*(1-.13*k),random.uniform(3,5),h*.36),DISTANT,'Mountains')
        crown((x,y,h*.9),3.2,250)
    # 六段分层瀑布是三维薄水幕，后续 UE 通过 Panner 驱动独立视觉材质。
    for x,y,top,width in ((-9,12,6.5,3.6),(-17,23,10,2.8),(9,14,6,3),(19,23,11,3),(5.8,28,5.8,1.8),(-6.5,6.5,1.5,2.5)):
        waterfall(x,y,top,width)
    # 远景中轴小宫殿强调月门后的深度。
    rock((0,48,4.2),(4.2,4.2,6.2),DISTANT,'Mountains',2026)
    box('Architecture',(0,48,10.65),(2.3,2,.35),STONE)
    for j in range(4):
        z=11+j*1.5
        box('Architecture',(0,48,z),(1.5-j*.2,1.3-j*.15,.55),STONE)
        for k in range(5):
            w=(2-j*.2)-k*.2
            box('Architecture',(0,48,z+.62+k*.12),(w,w*.7,.08),JADE)
    tube('Architecture',[(0,48,17),(0,48,18.2)],.09,GOLD,8,.1)


def waterfall(x,y,top,width):
    """弯曲水幕网格与水花几何，UV.y 沿落水方向供 UE 动态材质使用。"""
    verts=[]
    nx,ny=18,20
    for j in range(ny+1):
        t=j/ny
        for i in range(nx+1):
            u=i/nx
            verts.append((x+(u-.5)*width*(1+.15*t),y-.8*t+math.sin(u*30+t*4)*.065,top*(1-t)-.47))
    faces=[(j*(nx+1)+i,(j+1)*(nx+1)+i,(j+1)*(nx+1)+i+1,j*(nx+1)+i+1) for j in range(ny) for i in range(nx)]
    batch('Waterfalls').add(verts,faces,FALL,[(i/nx,j/ny) for j in range(ny+1) for i in range(nx+1)])
    for k in range(36):
        ellipsoid('Waterfalls',(x+random.uniform(-width*.65,width*.65),y-.9+random.uniform(-.5,.5),-.46+random.uniform(0,.15)),(random.uniform(.06,.23),random.uniform(.08,.2),random.uniform(.05,.17)),FOAM,10,6)


def fern(center,scale=1):
    """羽状蕨叶由实际小叶组成，用于前景和岩石缝。"""
    x,y,z=center
    for j in range(7):
        angle=TAU*j/7
        points=[(x+math.cos(angle)*scale*k/7,y+math.sin(angle)*scale*k/7,z+.55*scale*math.sin(k/7*math.pi*.8)) for k in range(8)]
        tube('Foliage',points,.012*scale,LEAF,5,.3)
        for k in range(1,7):
            p=Vector(points[k])
            direction=Vector((math.cos(angle),math.sin(angle),0))
            cross=Vector((-math.sin(angle),math.cos(angle),0))
            for side in (-1,1):
                end=p+direction*.12*scale+cross*(side*.22*scale*(1-k/8))
                leaf('Foliage',tuple(p),tuple(end),.05*scale,LEAF,.035*scale)


def flowers():
    """水面荷叶、白莲花及前景垂藤；中央平台留出角色空间。"""
    for j in range(66):
        x,y=random.uniform(-18,18),random.uniform(-9,12)
        if x*x+y*y<64:continue
        r=random.uniform(.16,.45)
        ring('WaterPlants',(x,y,-.44),0,r,.018,LEAF,20,a0=.1,a1=TAU-.1)
        if j%4==0:
            for k in range(9):
                a=TAU*k/9
                leaf('WaterPlants',(x,y,-.38),(x+math.cos(a)*r*.8,y+math.sin(a)*r*.8,-.26),r*.24,PETAL,.23)
            ellipsoid('WaterPlants',(x,y,-.25),(.05,.05,.045),GOLD,12,6)
    for side in (-1,1):
        for j in range(11):
            tree((side*random.uniform(17,22),random.uniform(-9,10),0),random.uniform(10,16),3.2)
        for j in range(16):
            x=side*random.uniform(7,22)
            y=random.uniform(-10,5)
            fern((x,y,0),random.uniform(.8,1.7))
        # 边框下垂藤蔓，视线窗口真实留在中央。
        for j in range(15):
            x=side*random.uniform(13,19)
            y=random.uniform(-3,14)
            z=random.uniform(11,17)
            points=[(x+.15*math.sin(k),y,z-k*.65) for k in range(9)]
            tube('Vines',points,.03,BARK,6,.3)
            for k,p in enumerate(points[1:]):
                leaf('Foliage',p,(p[0]+side*.6,p[1]-.2,p[2]-.15),.15,LEAF,.05)


def creature(kind, origin, scale, facing):
    """用连续雕刻体块与独立浮雕细节制作神兽；不构成可选角色或权威实体。

    原点处的单位兽体朝 +Y。通过本地转换形成左右相向构图；主体做体素
    融合消除互插体块的内部表面，鹿角、羽毛、鬃毛、鳞纹单独保留细节。
    """
    group='Sculpt_'+kind
    def p(v):
        x,y,z=v;c,s=math.cos(facing),math.sin(facing)
        return(origin[0]+scale*(x*c-y*s),origin[1]+scale*(x*s+y*c),origin[2]+scale*z)
    def ball(c,r,mat=SCULPT,detail=False):
        ellipsoid('StatueDetails' if detail else group,p(c),tuple(v*scale for v in r),mat,24,14,facing)
    def line(points,r,mat=SCULPT,detail=False,taper=.2):
        tube('StatueDetails' if detail else group,[p(v) for v in points],r*scale,mat,10,taper)
    if kind=='Phoenix':
        ball((0,0,1.1),(.5,.85,.95))
        line([(0,.35,1.4),(0,.7,2.1),(0,.82,2.6)],.26)
        ball((0,.87,2.65),(.27,.34,.28))
        line([(0,1,2.65),(0,1.5,2.6)],.16,taper=.01)
        for side in (-1,1):
            line([(side*.2,0,1.5),(side*1.15,.1,2.05),(side*2.1,.2,2.65)],.2)
            for j in range(16):
                t=j/15
                start=p((side*(.4+1.45*t),.15,1.7+.8*t))
                end=p((side*(1.1+1.8*t),-.8+.3*t,1.25+1.6*t))
                leaf('StatueDetails',start,end,.18*scale,SCULPT,.08*scale)
                line([(side*(.4+1.45*t),.16,1.72+.8*t),(side*(1.1+1.8*t),-.79+.3*t,1.28+1.6*t)],.018,GOLD,True,1)
            line([(side*.22,.08,.5),(side*.22,.32,.12)],.09)
        for j in range(12):
            t=(j-5.5)/5.5
            line([(t*.22,-.4,1),(t*.6,-1.2,.45),(t*.8,-2.3,.25),(t*1.05,-3,.5)],.085,SCULPT,True,.15)
        for j in range(5):line([(0,.7,2.8),((j-2)*.18,.5,3.2),((j-2)*.26,.7,3.55)],.035,SCULPT,True,.01)
    elif kind=='Dragon':
        body=[(0,-1.2,.2),(-.7,-.7,.6),(-1,-.2,1.2),(-.8,.15,1.9),(-.3,.15,2.5),(0,.2,2.85),(.1,.65,3)]
        line(body,.52,taper=.68)
        ball((.1,.75,3),(.44,.6,.37))
        ball((.1,1.25,2.94),(.36,.5,.24))
        ball((.1,1.33,2.72),(.32,.44,.11))
        for side in (-1,1):
            line([(side*.25,.63,3.25),(side*.48,.25,3.65),(side*.65,.05,4)],.1,SCULPT,True,.03)
            line([(side*.35,1.3,2.9),(side*.68,1.85,3.03),(side*1.02,2,3.35)],.025,GOLD,True,.08)
            line([(-.5,.0,1.7),(side*.8,.6,1.05),(side*.85,1.1,.45)],.19)
            ball((side*.85,1.15,.4),(.34,.42,.16))
        for j in range(12):
            t=j/11
            line([(-.5-t*.22,.03,1.6+t*1.3),(-.8-t*.5,-.2,1.8+t*1.3)],.13,SCULPT,True,.04)
        for j in range(45):
            t=j/44
            c=Vector(body[min(len(body)-2,int(t*(len(body)-1)))])
            for side in (-1,1):
                line([(c.x+side*.3,c.y-.3,c.z),(c.x+side*.44,c.y,c.z+.12),(c.x+side*.3,c.y+.25,c.z+.2)],.022,JADE,True,1)
    elif kind=='DragonTurtle':
        ball((0,-.1,.75),(.93,1.18,.65))
        ball((0,-.1,1.1),(1.02,1.28,.7),JADE,True)
        line([(0,.7,.9),(0,1.2,1.6),(0,1.65,1.85)],.28)
        ball((0,1.85,1.85),(.34,.5,.3))
        ball((0,2.18,1.77),(.28,.4,.2))
        for side in (-1,1):
            for y in (-.7,.65):
                line([(side*.65,y,.6),(side*1.15,y+.1,.25)],.23)
                ball((side*1.2,y+.2,.18),(.33,.4,.15))
            line([(side*.22,1.6,2),(side*.36,1.3,2.3),(side*.45,1,2.4)],.075,SCULPT,True,.02)
        for j in range(7):
            a=TAU*j/7
            points=[(.76*math.cos(a),-.1+.93*math.sin(a),1.5),(.3*math.cos(a),-.1+.36*math.sin(a),1.76),(0,-.1,1.8)]
            line(points,.03,GOLD,True,1)
        line([(0,-1,.65),(.25,-1.5,.3),(.4,-2,.4)],.17)
    else:
        is_deer=kind=='Deer'
        ball((0,-.05,1.0),(.48,.98,.52))
        ball((0,.62,1.16),(.5,.5,.61))
        headz=2.1 if is_deer else 1.7
        line([(0,.45,1.35),(0,.73,headz-.17)],.27 if is_deer else .37)
        ball((0,.86,headz),(.29 if is_deer else .46,.46,.34))
        ball((0,1.21,headz-.12),(.22 if is_deer else .34,.39,.23))
        if not is_deer:ball((0,1.18,headz-.32),(.33,.35,.12))
        for side in (-1,1):
            for y in (-.65,.58):
                line([(side*.33,y,.9),(side*.43,y+.16,.45),(side*.42,y+.3,.13)],.1 if is_deer else .21)
                ball((side*.42,y+.37,.1),(.11,.15,.12) if is_deer else (.28,.38,.16))
                if not is_deer:
                    for k in range(3):line([(side*.42+(k-1)*.1,y+.58,.16),(side*.42+(k-1)*.1,y+.7,.11)],.038,GOLD,True,.05)
            ball((side*.29,.78,headz+.27),(.13,.17,.22))
            ball((side*(.24 if is_deer else .36),1.1,headz+.04),(.055,.08,.045),GOLD,True)
            if is_deer or kind=='Qilin':
                horn=[(side*.2,.68,headz+.3),(side*.36,.48,headz+.75),(side*.55,.3,headz+1.25),(side*.73,.35,headz+1.6)]
                line(horn,.075,SCULPT,True,.08)
                for k in (1,2):line([horn[k],(horn[k][0]+side*.28,horn[k][1]+.15,horn[k][2]+.34)],.045,SCULPT,True,.01)
        line([(0,-.8,1),(.55,-1.1,.65),(.9,-1.5,.85),(.85,-1.8,1.2)],.11)
        if kind=='Tiger':
            for j in range(8):
                y=-.75+j*.18
                for side in (-1,1):line([(side*.12,y,1.53),(side*.45,y+.08,1.35),(side*.49,y+.16,1.07)],.026,JADE,True,.3)
            line([(-.24,1.44,1.66),(0,1.51,1.76),(.24,1.44,1.66)],.04,JADE,True,1)
        if kind=='Qilin':
            for j in range(18):
                a=TAU*j/18
                start=(math.cos(a)*.43,.72,1.72+math.sin(a)*.35)
                line([start,(start[0]*1.4,.32,start[2]+.1),(start[0]*1.25,-.08,start[2]+.25)],.11,SCULPT,True,.02)
            for j in range(18):
                y=-.72+(j//6)*.38;a=(j%6)*TAU/6
                line([(.43*math.cos(a),y,.99+.5*math.sin(a)),(.46*math.cos(a),y+.15,1.03+.5*math.sin(a)),(.42*math.cos(a),y+.3,1+.5*math.sin(a))],.026,GOLD,True,1)
    # 留下明确雕塑名称，融合只对本兽主体执行，避免破坏羽毛、角与纹饰。
    obj=BATCHES.pop(group).object()
    obj.name='Sculpt_DBA_'+kind
    bpy.context.view_layer.objects.active=obj
    obj.select_set(True)
    remesh=obj.modifiers.new('ContinuousStoneSculpt','REMESH')
    remesh.mode='VOXEL';remesh.voxel_size=.065*scale;remesh.use_smooth_shade=True
    bpy.ops.object.modifier_apply(modifier=remesh.name)
    smooth=obj.modifiers.new('StoneSurfaceRelaxation','SMOOTH');smooth.factor=.7;smooth.iterations=3
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    obj.data.materials.clear();obj.data.materials.append(SCULPT)
    for poly in obj.data.polygons:poly.material_index=0
    # 体素后给雕塑建立稳定 UV，保持制作件可独立编辑和导出。
    uv=obj.data.uv_layers.new(name='UVMap')
    for poly in obj.data.polygons:
        for loop_id in poly.loop_indices:
            v=obj.data.vertices[obj.data.loops[loop_id].vertex_index].co
            uv.data[loop_id].uv=(v.x/2,v.z/2)
    obj.select_set(False)
    return obj


def sculptures():
    """六尊石兽相向环抱，大小与前中后景位置按参考图的视觉层次安排。"""
    specs=[('Tiger',(-10,7,2.2),2.35,-.5),('Dragon',(-13,20,7),2.2,-.65),('Deer',(-6,24,10),1.8,-.45),('DragonTurtle',(10,17,6),2.4,.65),('Phoenix',(16,29,13),2.6,.15),('Qilin',(13,6,3),2.6,.5)]
    result=[]
    for kind,point,size,angle in specs:
        box('Architecture',(point[0],point[1],point[2]-.28),(size*1.25,size*1.2,.3),STONE)
        ring('ZodiacInlays',(point[0],point[1],point[2]+.04),size*.8,size*1.08,.04,GOLD)
        result.append(creature(kind,point,size,angle))
    return result


def lighting():
    """Blender 制作镜头与日光，UE 使用同一个米制视线合同重建光照。"""
    scene=bpy.context.scene
    world=bpy.data.worlds.new('SanctuarySky');scene.world=world;world.use_nodes=True
    world.node_tree.nodes['Background'].inputs['Color'].default_value=(.4,.61,.68,1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value=.6
    sky=world.node_tree.nodes.new('ShaderNodeTexSky');sky.sky_type='NISHITA'
    sky.sun_elevation=math.radians(35);sky.sun_rotation=math.radians(125)
    world.node_tree.links.new(sky.outputs['Color'],world.node_tree.nodes['Background'].inputs['Color'])
    sun=bpy.data.lights.new('SanctuaryMorningSun','SUN');sun.energy=2.4;sun.angle=.09
    obj=bpy.data.objects.new('SanctuaryMorningSun',sun);scene.collection.objects.link(obj)
    obj.rotation_euler=(math.radians(28),math.radians(-30),math.radians(-32))
    for name,location,power,size in [('SkyFill',(-12,-4,18),2600,16),('HeroKey',(3,-5,8),1400,8)]:
        data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
        light=bpy.data.objects.new(name,data);scene.collection.objects.link(light);light.location=location
        light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
    data=bpy.data.cameras.new('SanctuaryReferenceCamera');camera=bpy.data.objects.new('SanctuaryReferenceCamera',data)
    scene.collection.objects.link(camera);camera.location=(0,-23,11)
    camera.rotation_euler=(Vector((0,11,4.8))-camera.location).to_track_quat('-Z','Y').to_euler();data.lens=25
    data.clip_end=250;scene.camera=camera
    scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True
    prefs=bpy.context.preferences.addons['cycles'].preferences
    try:
        prefs.compute_device_type='OPTIX';prefs.get_devices()
        for device in prefs.devices:device.use=device.type!='CPU'
        scene.cycles.device='GPU'
    except Exception:
        scene.cycles.device='CPU'
    # 大气体积只用于 Blender 制作渲染；UE 由原生雾组件重建，不导出实体遮挡。
    bpy.ops.mesh.primitive_cube_add(size=2,location=(0,25,22))
    fog=bpy.context.object;fog.name='AuthoringAtmosphere';fog.scale=(65,70,35)
    fogmat=bpy.data.materials.new('AuthoringAtmosphere');fogmat.use_nodes=True
    fogmat.node_tree.nodes.clear()
    volume=fogmat.node_tree.nodes.new('ShaderNodeVolumeScatter')
    volume.inputs['Density'].default_value=.008;volume.inputs['Anisotropy'].default_value=.25
    volume.inputs['Color'].default_value=(.6,.76,.79,1)
    output=fogmat.node_tree.nodes.new('ShaderNodeOutputMaterial')
    fogmat.node_tree.links.new(volume.outputs['Volume'],output.inputs['Volume'])
    fog.data.materials.append(fogmat)
    scene.render.resolution_x=1664;scene.render.resolution_y=936;scene.render.resolution_percentage=100
    scene.world.color=(.4,.6,.7)
    scene.view_settings.view_transform='AgX'
    scene.view_settings.look='AgX - Medium High Contrast'
    scene.view_settings.exposure=.7
    scene.render.image_settings.file_format='PNG'
    scene.render.filepath=str(OUT/'Previews'/'ZodiacSanctuary.png')


def main():
    """一次完整生成与保存；失败保持异常，不用进程退出码掩盖 Blender 异常。"""
    global OUT,STONE,JADE,GOLD,MOSS,BARK,LEAF,LEAFLIGHT,SCULPT,DISTANT,FALL,FOAM,GLOW,PETAL
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',required=True)
    parser.add_argument('--skip-render',action='store_true')
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    OUT=Path(args.output);OUT.mkdir(parents=True,exist_ok=True)
    for part in ('Textures','Exports','Previews'):(OUT/part).mkdir(exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    stone=texture('T_DBA_Sanctuary_Stone',(.22,.29,.23),(.67,.65,.49))
    jade=texture('T_DBA_Sanctuary_Jade',(.045,.14,.115),(.21,.36,.27))
    moss=texture('T_DBA_Sanctuary_Moss',(.045,.09,.018),(.23,.34,.065))
    bark=texture('T_DBA_Sanctuary_Bark',(.08,.105,.043),(.30,.26,.14),'bark')
    leaves=texture('T_DBA_Sanctuary_Leaf',(.03,.095,.018),(.21,.34,.046),'leaf',512)
    STONE=material('Stone',(.52,.57,.44),image=stone)
    JADE=material('Jade',(.11,.27,.22),.43,.12,jade)
    GOLD=material('AncientGold',(.47,.38,.16),.38,.66)
    MOSS=material('Moss',(.11,.24,.035),.94,image=moss)
    BARK=material('Bark',(.18,.20,.09),.92,image=bark)
    LEAF=material('Leaf',(.07,.22,.025),.72,image=leaves)
    LEAFLIGHT=material('LeafLight',(.23,.39,.065),.68)
    SCULPT=material('CarvedStone',(.57,.61,.47),.65,image=stone)
    DISTANT=material('DistantStone',(.25,.39,.36),.85,image=stone)
    FALL=material('Waterfall',(.56,.78,.74),.23,.1,emission=.18)
    FOAM=material('Foam',(.72,.89,.81),.4,emission=.12)
    GLOW=material('LanternGlow',(.28,.76,.58),.42,emission=2)
    PETAL=material('LotusPetal',(.83,.87,.72),.42)
    water=material('Water',(.025,.22,.17),.16,.32)
    platform();moon_gate();terrain();flowers();statues=sculptures()
    box('Water',(0,12,-.6),(28,28,.035),water)
    objects=[b.object() for b in BATCHES.values()]+statues
    lighting()
    scene=bpy.context.scene
    scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
    manifest={'schema_version':1,'scene':'ZodiacSanctuary','reference':'User provided 2026-10-11','blender_version':bpy.app.version_string,'units':'meters','source_axes':{'right':'+X','background':'+Y','up':'+Z'},'character_foot_origin_meters':[0,0,0],'platform_radius_meters':7,'camera_meters':[0,-23,11],'camera_target_meters':[0,11,4.8],'objects':[],'materials':[]}
    for obj in objects:
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        file=OUT/'Exports'/(obj.name+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,global_scale=1,path_mode='COPY',embed_textures=False,use_mesh_modifiers=True,add_leaf_bones=False,bake_anim=False)
        manifest['objects'].append({'name':obj.name,'fbx':str(file.relative_to(OUT)).replace('\\','/'),'vertices':len(obj.data.vertices),'polygons':len(obj.data.polygons),'materials':[m.name for m in obj.data.materials]})
    for mat in MATERIALS:
        shader=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
        image=next((n.image for n in mat.node_tree.nodes if n.type=='TEX_IMAGE'),None)
        manifest['materials'].append({'name':mat.name,'base_color':list(shader.inputs['Base Color'].default_value)[:3],'roughness':shader.inputs['Roughness'].default_value,'metallic':shader.inputs['Metallic'].default_value,'emission_strength':shader.inputs['Emission Strength'].default_value,'texture':('Textures/'+image.name+'.png') if image else None,'two_sided':mat in (LEAF,LEAFLIGHT,FALL,PETAL)})
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'DBA_ZodiacSanctuary.blend'))
    (OUT/'SceneManifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    if not args.skip_render:bpy.ops.render.render(write_still=True)
    print('DBA_SANCTUARY_GENERATED '+json.dumps({'objects':len(objects),'vertices':sum(len(o.data.vertices) for o in objects),'blend':str(OUT/'DBA_ZodiacSanctuary.blend')},ensure_ascii=False))


if __name__=='__main__':
    main()
