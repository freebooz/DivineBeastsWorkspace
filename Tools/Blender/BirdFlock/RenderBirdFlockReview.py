#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Blender鸟群原始模型可视审核图制作，独立预览，不写回正式UE资产。

只读加载原有Blender源工程；渲染三个物种的低面数剪影、顶点色与大致展翼比例，
输出Contact Sheet帮助美术人工优选。预览不代表UE5.8 GPU/材质或Niagara验收。
"""
from __future__ import annotations
from pathlib import Path
import math

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock"


def main():
    import bpy
    from mathutils import Vector
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE/"DBA_BirdFlock_Models.blend"))
    scene=bpy.context.scene
    scene.render.engine="CYCLES"
    scene.cycles.samples=20
    scene.render.resolution_x=1500
    scene.render.resolution_y=700
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format="PNG"
    scene.render.film_transparent=False
    if scene.world is None:
        scene.world=bpy.data.worlds.new("BirdFlockPreviewWorld")
    scene.world.color=(0.035,0.04,0.06)
    scene.view_settings.view_transform="AgX"

    objs={n:bpy.data.objects.get("SM_DBA_Bird_"+n) for n in ("Seagull","Swallow","Eagle")}
    if any(ob is None for ob in objs.values()):
        raise RuntimeError("鸟群原始模型缺失")
    for name,x in (("Seagull",-2.7),("Swallow",0.0),("Eagle",2.7)):
        obj=objs[name]
        obj.location=(x,0,0.45)
        mat=bpy.data.materials.new("PreviewFeather_"+name)
        mat.use_nodes=True
        tree=mat.node_tree
        bsdf=next((n for n in tree.nodes if n.type=="BSDF_PRINCIPLED"),None)
        if bsdf is None: raise RuntimeError("Blender BSDF不存在")
        attr=tree.nodes.new("ShaderNodeVertexColor")
        attr.layer_name="BirdColorWingWeight"
        tree.links.new(attr.outputs["Color"],bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value=.73
        obj.data.materials.clear()
        obj.data.materials.append(mat)

    def area_light(x,y,z,power,size):
        data=bpy.data.lights.new("SoftBirdPreviewLight","AREA")
        data.energy=power
        data.shape="DISK"
        data.size=size
        ob=bpy.data.objects.new("BirdPreviewLight",data)
        scene.collection.objects.link(ob)
        ob.location=(x,y,z)
        target=Vector((0,0,.2))-ob.location
        ob.rotation_euler=target.to_track_quat("-Z","Y").to_euler()
    area_light(-3.5,-5,5,720,7)
    area_light(3.5,5,4,650,8)
    camera_data=bpy.data.cameras.new("BirdPreviewCamera")
    camera=bpy.data.objects.new("BirdPreviewCamera",camera_data)
    scene.collection.objects.link(camera)
    camera.location=(0,-10,6)
    target=Vector((0,0,.32))-camera.location
    camera.rotation_euler=target.to_track_quat("-Z","Y").to_euler()
    camera.data.type="ORTHO"
    camera.data.ortho_scale=10.6
    scene.camera=camera
    for n,x in (("SEAGULL",-2.7),("SWALLOW",0),("EAGLE",2.7)):
        curve=bpy.data.curves.new("BirdSpeciesLabel_"+n,type="FONT")
        curve.body=n
        curve.align_x="CENTER"
        curve.size=.27
        label=bpy.data.objects.new("BirdSpeciesLabel_"+n,curve)
        scene.collection.objects.link(label)
        label.location=(x,-1.15,-.75)
        label.rotation_euler=(math.radians(90),0,0)
    destination=SOURCE/"Previews"/"BirdFlockSpeciesContactSheet.png"
    destination.parent.mkdir(parents=True,exist_ok=True)
    scene.render.filepath=str(destination)
    bpy.ops.render.render(write_still=True)
    if not destination.is_file() or destination.stat().st_size<10000:
        raise RuntimeError("预览图没有成功落盘")
    print("BIRD_VISUAL_PREVIEW_SAVED",destination,destination.stat().st_size)

if __name__=="__main__":
    main()
