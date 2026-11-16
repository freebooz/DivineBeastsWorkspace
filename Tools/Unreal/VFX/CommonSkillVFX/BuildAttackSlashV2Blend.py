# -*- coding: utf-8 -*-
"""在空Blender场景建立攻击V2可编辑设计曲线、三维刀翼与三材质FBX。

造型针对确认稿左上弧刃；保留控制点、真实厚度、低频云卷与渐细流带。
输出只是DCC制作源，UE资产由后续Monolith导入；不创建游戏工程或播放器。
"""
from pathlib import Path
import json
import math
import argparse,shutil
from datetime import datetime,timezone
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V2/AttackSlash'
assert (OUT/'TextureContract.json').is_file(),'先生成纹理合同'
args=argparse.ArgumentParser();args.add_argument('--revise-own-output',action='store_true');options=args.parse_args(__import__('sys').argv[__import__('sys').argv.index('--')+1:] if '--' in __import__('sys').argv else [])
if (OUT/'AttackSlashV2.blend').exists():
    assert options.revise_own_output,'拒绝覆盖已保存源文件'
    old=json.loads((OUT/'ShapeContract.json').read_text(encoding='utf8'));assert old['outputMesh']=='SM_DBA_AttackSlash_V2' and old['manualAccepted'] is False
    revision=OUT/'Revisions'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ');revision.mkdir(parents=True)
    for name in ('AttackSlashV2.blend','SM_DBA_AttackSlash_V2.fbx','ShapeContract.json'):shutil.copy2(OUT/name,revision/name)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
if scene.world is None:scene.world=bpy.data.worlds.new('SlashV2_SourceWorld')
scene.world.color=(.10,.12,.15)
controls=bpy.data.collections.new('Editable_Bezier_Design');scene.collection.children.link(controls);controls.hide_render=True
render=bpy.data.collections.new('Runtime_Mesh');scene.collection.children.link(render)
OUTER=[(-205,-155),(-45,-235),(150,-210),(270,-95),(280,50),(190,185),(15,235),(-140,205),(-220,145)]
INNER=[(-205,-155),(-70,-165),(35,-140),(130,-65),(145,35),(100,110),(-5,170),(-120,195),(-220,145)]


def spatial(p,height=0):return Vector((p[0],p[1],100-.08*p[1]+height))
def sample(points,t):
    """Catmull控制点转连续曲线，与保存Bezier的切向一致。"""
    f=min(.999999,max(0,t))*(len(points)-1);i=int(f);q=f-i
    p0=Vector(points[max(0,i-1)]);p1=Vector(points[i]);p2=Vector(points[min(len(points)-1,i+1)]);p3=Vector(points[min(len(points)-1,i+2)])
    return .5*((2*p1)+(-p0+p2)*q+(2*p0-5*p1+4*p2-p3)*q*q+(-p0+3*p1-3*p2+p3)*q*q*q)


def source_curve(name,points,height=0):
    data=bpy.data.curves.new(name,'CURVE');data.dimensions='3D';data.resolution_u=24
    s=data.splines.new('BEZIER');s.bezier_points.add(len(points)-1)
    pts=[spatial(p,height) for p in points]
    for i,(b,p) in enumerate(zip(s.bezier_points,pts)):
        delta=(pts[min(len(pts)-1,i+1)]-pts[max(0,i-1)])/6
        b.co=p;b.handle_left_type='FREE';b.handle_right_type='FREE';b.handle_left=p-delta;b.handle_right=p+delta
    obj=bpy.data.objects.new(name,data);controls.objects.link(obj);obj.hide_set(True);return obj


def material(name,color,emission):
    mat=bpy.data.materials.new(name);mat.use_nodes=True
    bsdf=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    bsdf.inputs['Base Color'].default_value=(*color,1);bsdf.inputs['Roughness'].default_value=.25
    if 'Emission Color' in bsdf.inputs:bsdf.inputs['Emission Color'].default_value=(*color,1);bsdf.inputs['Emission Strength'].default_value=emission
    return mat


materials=[material('M_Source_SlashBody',(1,.21,.035),.8),material('M_Source_SilkStreams',(1,.45,.09),2),material('M_Source_CloudAndRim',(1,.75,.28),3)]
verts=[];faces=[];face_uv=[];face_material=[]


def patch(fn,nu,nv,slot):
    offset=len(verts)
    for i in range(nu+1):
        for j in range(nv+1):verts.append(tuple(fn(.0005+.999*i/nu,j/nv)))
    for i in range(nu):
        for j in range(nv):
            a=offset+i*(nv+1)+j;b=a+nv+1
            faces.extend(((a,b,a+1),(a+1,b,b+1)))
            vv=lambda k:(1+math.cos(k/nv*math.tau))*.5
            face_uv.extend((((i/nu,vv(j)),((i+1)/nu,vv(j)),(i/nu,vv(j+1))),((i/nu,vv(j+1)),((i+1)/nu,vv(j)),((i+1)/nu,vv(j+1)))))
            face_material.extend((slot,slot))


source_curve('Shape_OutlineOuter',OUTER);source_curve('Shape_OutlineInner',INNER)


def blade(u,v):
    o=sample(OUTER,u);i=sample(INNER,u);center=(o+i)*.5;half=(o-i)*.5
    p=center+half*math.cos(v*math.tau)
    return spatial(p,(4+11*math.sin(math.pi*u))*math.sin(v*math.tau))


patch(blade,180,16,0)


def ribbon(name,points,width,slot,height=0,flatten=.15):
    source_curve(name,points,height)
    def fn(u,v):
        p=spatial(sample(points,u),height)
        tangent=spatial(sample(points,min(1,u+.0001)),height)-spatial(sample(points,max(0,u-.0001)),height)
        tangent.normalize();up=Vector((0,.08,1)).normalized();side=tangent.cross(up).normalized();up=side.cross(tangent).normalized()
        radius=.12+width*math.sin(math.pi*u)**.65
        return p+side*math.cos(v*math.tau)*radius+up*math.sin(v*math.tau)*radius*flatten
    patch(fn,120,8,slot)


streams=[
('Flow_OuterSilk',[(-270,-165),(-140,-223),(40,-249),(220,-177),(302,-30),(265,115),(140,220)],8,10),
('Flow_InnerSilk',[(-230,-148),(-95,-179),(57,-151),(194,-68),(207,56),(127,159),(-35,214)],10,15),
('Flow_TailSilk',[(-300,-164),(-192,-198),(-80,-198),(48,-163),(138,-93),(157,-8)],5,5),
('Flow_InnerBlade',[(-160,-151),(-45,-164),(73,-132),(166,-48),(171,65),(70,157)],16,8)]
for name,points,width,height in streams:ribbon(name,points,width,1,height,.12)
ribbon('Edge_OuterHighlight',OUTER,1.4,2,3,.7)
ribbon('Edge_InnerHighlight',INNER,1.0,2,3,.6)
clouds=[
('Cloud_LargeCurl',[(87,-115),(140,-99),(185,-78),(211,-49),(213,-29),(200,-15),(181,-20),(175,-37),(186,-47),(200,-43),(204,-31)],10),
('Cloud_UpperCurl',[(191,-45),(189,-15),(176,11),(153,22),(135,11),(135,-8),(150,-19),(164,-11),(166,1)],8.5),
('Cloud_LowerWisp',[(52,-124),(95,-118),(132,-97),(155,-72),(153,-53),(137,-45)],6)]
for name,points,width in clouds:ribbon(name,points,width,2,21,.12)

data=bpy.data.meshes.new('SM_DBA_AttackSlash_V2');data.from_pydata(verts,[],faces);data.update()
obj=bpy.data.objects.new('SM_DBA_AttackSlash_V2',data);render.objects.link(obj)
for mat in materials:data.materials.append(mat)
uv=data.uv_layers.new(name='FlowUV')
for polygon,coords,slot in zip(data.polygons,face_uv,face_material):
    polygon.material_index=slot;polygon.use_smooth=True
    for loop,coord in zip(polygon.loop_indices,coords):uv.data[loop].uv=coord
obj['Units']='centimeters';obj['Reference']='SSLM_CommonVFX_Attack_V1.png panel upper-left';obj['ManualAccepted']=False
for o in bpy.context.selected_objects:o.select_set(False)
obj.select_set(True);bpy.context.view_layer.objects.active=obj
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_DBA_AttackSlash_V2.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,global_scale=1,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',use_mesh_modifiers=True,add_leaf_bones=False,bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AttackSlashV2.blend'))
report=dict(reference='SSLM_CommonVFX_Attack_V1.png upper-left',outputMesh=obj.name,editableCurves=len([o for o in bpy.data.objects if o.type=='CURVE']),vertices=len(verts),triangles=len(faces),materialSlots=3,units='centimeters',nativeImported=False,manualAccepted=False)
(OUT/'ShapeContract.json').write_text(json.dumps(report,indent=2),encoding='utf8')
print('SLASH_V2_DCC_SAVED',json.dumps(report))
