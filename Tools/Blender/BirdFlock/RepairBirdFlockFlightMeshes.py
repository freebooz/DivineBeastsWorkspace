#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群可视资源修正：去除反向重复翼面/退化面并保留RGB羽色与A振翅权重。

仅读取本项目已有生成源，输出独立RefinedMeshes，不覆盖旧Blender/FBX或其他用户素材。
双面渲染由UE材质负责，几何不得再创建共面反向三角面以免法线抵消和深度闪烁。
"""
from pathlib import Path
import hashlib
import json
import bpy
import bmesh

ROOT=Path(__file__).resolve().parents[3]
SRC=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock'
OUT=SRC/'RefinedMeshes'


def main():
    bpy.ops.wm.open_mainfile(filepath=str(SRC/'DBA_BirdFlock_Models.blend'))
    OUT.mkdir(parents=True,exist_ok=True)
    rows=[]
    for species in ('Seagull','Swallow','Eagle'):
        obj=bpy.data.objects.get('SM_DBA_Bird_'+species)
        if obj is None:
            raise RuntimeError('原始生成鸟类不存在：'+species)
        mesh=obj.data
        before=len(mesh.polygons)
        bm=bmesh.new();bm.from_mesh(mesh)
        seen=set();dups=[]
        for face in bm.faces:
            key=tuple(sorted(v.index for v in face.verts))
            if key in seen:
                dups.append(face)
            else:
                seen.add(key)
        bmesh.ops.delete(bm,geom=dups,context='FACES_ONLY')
        bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.000001)
        degenerate=[f for f in bm.faces if f.calc_area()<1e-10]
        bmesh.ops.delete(bm,geom=degenerate,context='FACES_ONLY')
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(mesh);bm.free();mesh.update()
        attr=mesh.color_attributes.get('BirdColorWingWeight')
        if attr is None or len(attr.data)==0:
            raise RuntimeError('几何清理丢失顶点羽色和振翅权重')
        alpha=[float(v.color[3]) for v in attr.data]
        unique_rgb={tuple(round(c,3) for c in v.color[:3]) for v in attr.data}
        if max(alpha)<.8 or min(alpha)>.05 or len(unique_rgb)<4:
            raise RuntimeError('羽色/翼部权重不完整：'+species)
        # 显式设置活动渲染顶点色层，防止FBX只导出默认空白白色层。
        mesh.color_attributes.active_color=attr
        for old in bpy.context.selected_objects:
            old.select_set(False)
        obj.select_set(True);bpy.context.view_layer.objects.active=obj
        target=OUT/('SM_DBA_Bird_'+species+'_Flight.fbx')
        bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,
            axis_forward='X',axis_up='Z',apply_unit_scale=True,
            apply_scale_options='FBX_SCALE_UNITS',bake_anim=False,add_leaf_bones=False,
            mesh_smooth_type='FACE',colors_type='LINEAR')
        rows.append({'species':species,'sourceFaces':before,'faces':len(mesh.polygons),
            'removedReverseFaces':len(dups),'vertices':len(mesh.vertices),
            'wingAlphaMin':min(alpha),'wingAlphaMax':max(alpha),'colorVariants':len(unique_rgb),
            'file':target.name,'bytes':target.stat().st_size,
            'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
        print('BIRD_FLIGHT_MESH_REPAIRED',json.dumps(rows[-1]))
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'DBA_BirdFlock_Flight.blend'),compress=True)
    (OUT/'FlightMeshManifest.json').write_text(json.dumps({'version':1,'meshes':rows},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_FLIGHT_SOURCE_REPAIR_PASSED',len(rows))


if __name__=='__main__':
    main()
