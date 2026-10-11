#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Blender 5.2 鸟群静态网格及纹理源文件生成器（项目层 SourceArt）。

使用独立 Blender 后台进程运行，不修改 UE 已保存资产；在失败时不删除原有资源。
输出三种鸟类 FBX、一个可编辑 blend 工程、三幅无颗粒柔和羽纹原图和 SHA256 清单。
模型方向：X 前进、Y 左右展翼、Z 上；Vertex Color RGB 为部位色，A 为振翅形变权重。
注意：此模型为低面数实时环境鸟群资产，不是近景影视级羽毛角色。
"""
from __future__ import annotations
import colorsys
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock"
FBX = SRC / "Meshes"
TEX = SRC / "Textures"

# 物种参数均属于项目表现层；通用平台层不认识物种标识。
PROFILES = {
    "Seagull": {"length": .45, "span": 1.55, "chord": .26, "torso": (.37, .13, .12),
                "body": (.83, .87, .89), "wing": (.82, .86, .90), "tip": (.50, .57, .64),
                "beak": (.94, .61, .21), "head": (.94, .95, .94), "tail": .30,
                "camera": "湖面滑翔海鸥"},
    "Swallow": {"length": .31, "span": .97, "chord": .14, "torso": (.25, .079, .073),
                 "body": (.14, .20, .29), "wing": (.12, .17, .25), "tip": (.10, .15, .22),
                 "beak": (.24, .18, .13), "head": (.14, .18, .24), "tail": .47,
                 "camera": "疾速掠过林冠的燕子"},
    "Eagle": {"length": .62, "span": 2.55, "chord": .49, "torso": (.56, .20, .18),
               "body": (.30, .19, .12), "wing": (.37, .25, .17), "tip": (.19, .15, .11),
               "beak": (.77, .61, .32), "head": (.67, .59, .43), "tail": .33,
               "camera": "山谷滑翔雄鹰"},
}


def _rgba(color, weight=0.0):
    return (*color, max(0.0, min(1.0, weight)))


def build_bird(bpy, name, settings):
    verts, uvcoords, colors, faces = [], [], [], []

    def vertex(pos, uv, rgb, weight=0.0):
        verts.append(tuple(pos))
        uvcoords.append(tuple(uv))
        colors.append(_rgba(rgb, weight))
        return len(verts)-1

    def face(a, b, c):
        faces.append((a, b, c))

    def ellipsoid(center, radius, tint, u=20, v=12):
        start = len(verts)
        for j in range(v+1):
            lat = math.pi*j/v
            for i in range(u+1):
                lng = math.tau*i/u
                x = center[0] + radius[0]*math.cos(lat)
                y = center[1] + radius[1]*math.sin(lat)*math.cos(lng)
                z = center[2] + radius[2]*math.sin(lat)*math.sin(lng)
                vertex((x,y,z), (i/u,j/v), tint)
        for j in range(v):
            for i in range(u):
                a=start+j*(u+1)+i
                b=a+(u+1)
                face(a,b,a+1)
                face(a+1,b,b+1)

    ellipsoid((0,0,0),settings["torso"],settings["body"])
    ellipsoid((settings["torso"][0]*.72,0,.028),(.135,settings["torso"][1]*.75,
              settings["torso"][2]*.73),settings["head"],u=16,v=9)

    # 鸟喙采用带厚度的楔形，眼睛两侧保留轮廓。
    beak_x=settings["torso"][0]*.72+.13
    a=vertex((beak_x,0,.02),(.25,.50),settings["beak"])
    b=vertex((beak_x+.105,0,-.014),(.75,.5),settings["beak"])
    c=vertex((beak_x-.015,-.038,.013),(0,0),settings["beak"])
    d=vertex((beak_x-.015,.038,.013),(0,1),settings["beak"])
    for x,y,z in [(a,b,c),(a,d,b),(a,c,d),(b,d,c)]: face(x,y,z)
    for side in (-1,1):
        ellipsoid((settings["torso"][0]*.72+.085,side*settings["torso"][1]*.53,
                   .078),(.017,.012,.017),(.045,.052,.058),u=8,v=5)

    # 展翼从翼根向翼尖收尖并后掠，GPU 材质通过 VertexColor.A 控制翼尖振翅。
    for side in (-1,1):
        numspan=12
        numchord=4
        bank = 0.025
        base=len(verts)
        for j in range(numspan+1):
            t=j/numspan
            width=settings["span"]*.5
            y=side*(settings["torso"][1]*.60+width*t)
            sweep=0.07-(.14 if name=="Swallow" else .23)*t
            local_chord=settings["chord"] * max(.12, (1-t)**(.48 if name=="Eagle" else .72))
            for i in range(numchord+1):
                c=i/numchord
                x=sweep-local_chord*c
                z=.034+.08*t*t + .021*math.sin(math.pi*c)
                if name=="Eagle": z+=.03*t
                tint_mix=min(1,max(0,(t-.63)/.37))
                color=tuple(settings["wing"][k]*(1-tint_mix)+settings["tip"][k]*tint_mix
                            for k in range(3))
                uv=(c,t)
                vertex((x,y,z),uv,color,t*.92)
        for j in range(numspan):
            for i in range(numchord):
                a=base+j*(numchord+1)+i
                b=a+(numchord+1)
                if side==1:
                    face(a,a+1,b)
                    face(a+1,b+1,b)
                else:
                    face(a,b,a+1)
                    face(a+1,b,b+1)
        # 翼片双面额外一组反向面，保证渲染管线未双面时也有形状。
        start_faces=len(faces)-numspan*numchord*2
        for fi in range(start_faces,len(faces)):
            old=faces[fi]
            faces.append((old[2],old[1],old[0]))

    # 燕子叉尾更长，雄鹰尾部较宽；不用独立骨骼组件。
    tailend= -settings["torso"][0]-settings["tail"]
    for side in (-1,1):
        root=vertex((-settings["torso"][0]*.74,side*.035,.0),(.1,.3),settings["body"])
        tip_y=side*(.17 if name=="Swallow" else .11)
        tip=vertex((tailend,tip_y,-.025),(.95,.8),settings["wing"])
        inner=vertex((tailend+(.17 if name=="Swallow" else .04),0,-.027),
                     (.55,.5),settings["wing"])
        face(root,tip,inner)
        face(inner,tip,root)

    mesh=bpy.data.meshes.new("SM_DBA_Bird_"+name+"_Mesh")
    mesh.from_pydata(verts,[],faces)
    mesh.update()
    obj=bpy.data.objects.new("SM_DBA_Bird_"+name,mesh)
    bpy.context.collection.objects.link(obj)

    uvs=mesh.uv_layers.new(name="UV0")
    attr=mesh.color_attributes.new(name="BirdColorWingWeight",type="FLOAT_COLOR",domain="CORNER")
    for polygon in mesh.polygons:
        for li in polygon.loop_indices:
            vi=mesh.loops[li].vertex_index
            uvs.data[li].uv=uvcoords[vi]
            attr.data[li].color=colors[vi]
        polygon.use_smooth=True

    default=bpy.data.materials.new("M_Source_"+name)
    default.diffuse_color=(*settings["body"],1)
    mesh.materials.append(default)
    return obj


def make_texture_png(path, name, settings, size=512):
    """不添加砂砾/噪点；极低对比羽纹只提供柔和的微观色彩层次。"""
    rgb=settings["wing"]
    def chunk(tag,data):
        return struct.pack("!I",len(data))+tag+data+struct.pack("!I",zlib.crc32(tag+data)&0xffffffff)
    scan=[]
    for y in range(size):
        scan.append(b"\x00")
        v=y/(size-1)
        buf=bytearray()
        for x in range(size):
            u=x/(size-1)
            # 纹理尽量中性，项目鸟种特征由顶点色表现，减少多材质开销。
            bar=.018*math.sin((u+v*.10)*math.pi*12)**2
            shade=.96+.035*(1-v)-bar
            for c in rgb:
                buf.append(max(0,min(255,round(255*(.93 +.07*c)*shade))))
            buf.append(255)
        scan.append(bytes(buf))
    ihdr=struct.pack("!2I5B",size,size,8,6,0,0,0)
    data=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",ihdr)+chunk(b"IDAT",zlib.compress(b"".join(scan),9))+chunk(b"IEND",b"")
    path.write_bytes(data)


def main():
    try:
        import bpy
    except ImportError as error:
        raise RuntimeError("本脚本必须由 Blender Python 执行") from error
    if not bpy.app.version[:2] >= (4,0):
        raise RuntimeError("Blender版本过低，至少4.0")
    for d in (SRC,FBX,TEX):
        d.mkdir(parents=True,exist_ok=True)

    # 仅在独立无用户场景的后台 Blender 进程执行。
    bpy.ops.wm.read_factory_settings(use_empty=True)
    generated=[]
    objs=[]
    for name,settings in PROFILES.items():
        obj=build_bird(bpy,name,settings)
        objs.append(obj)
        for previous in bpy.context.selected_objects:
            previous.select_set(False)
        obj.select_set(True)
        bpy.context.view_layer.objects.active=obj
        target=FBX/("SM_DBA_Bird_"+name+".fbx")
        bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,
                                 apply_unit_scale=True,apply_scale_options="FBX_SCALE_UNITS",
                                 axis_forward="X",axis_up="Z",
                                 add_leaf_bones=False,bake_anim=False,
                                 use_mesh_modifiers=True,mesh_smooth_type="FACE")
        generated.append(target)
        tex=TEX/("T_DBA_Bird_"+name+"_Albedo_Source.png")
        make_texture_png(tex,name,settings)
        generated.append(tex)
        print("BIRD_SOURCE_EXPORTED",name,"verts",len(obj.data.vertices),"faces",len(obj.data.polygons))

    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
    blend=SRC/"DBA_BirdFlock_Models.blend"
    bpy.context.scene.unit_settings.system="METRIC"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend),compress=True)
    generated.append(blend)

    report={
        "schemaVersion":1,
        "generator":"Tools/Blender/BirdFlock/GenerateBirdFlockModels.py",
        "blenderVersion":bpy.app.version_string,
        "note":"环境远景鸟类低面数资源。顶点RGB=颜色，A=振翅权重；未经人工视觉验收。",
        "species":[{
            "name":name, "mesh":"SM_DBA_Bird_"+name+".fbx",
            "texture":"T_DBA_Bird_"+name+"_Albedo_Source.png",
            "birdLengthM":settings["length"],
            "wingspanM":settings["span"]
        } for name,settings in PROFILES.items()],
        "files":[{"path":str(f.relative_to(SRC)).replace("\\","/"),
                  "bytes":f.stat().st_size,
                  "sha256":hashlib.sha256(f.read_bytes()).hexdigest()} for f in generated]
    }
    (SRC/"BirdFlockSourceManifest.json").write_text(
        json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("BIRD_SOURCE_MANIFEST",SRC/"BirdFlockSourceManifest.json")


if __name__=="__main__":
    main()
