# -*- coding: utf-8 -*-
"""定向飞弹可编辑三维梭形弹头；纯美术源，不生成权威飞行对象。"""
from pathlib import Path
import argparse,json,math,sys,shutil
from datetime import datetime,timezone
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile'
p=argparse.ArgumentParser();p.add_argument('--revise-own-output',action='store_true');a=p.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
if (OUT/'DirectedMissileV1.blend').exists():
    assert a.revise_own_output
    backup=OUT/'Revisions'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ');backup.mkdir(parents=True)
    for n in ('DirectedMissileV1.blend','SM_DBA_DirectedMissile_V1.fbx','ReferenceGuide.json'):shutil.copy2(OUT/n,backup/n)
OUT.mkdir(parents=True,exist_ok=True);bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01;scene.world=bpy.data.worlds.new('SourceWorld')
cam=Vector((850,-1150,1250));look=Vector((0,0,100));fwd=(look-cam).normalized();right=Vector((0,0,1)).cross(fwd).normalized();up=fwd.cross(right).normalized();fov=45;focal=1106/(2*math.tan(math.radians(fov)/2))
def unproject(pixel,z):
    ray=fwd+right*((pixel[0]-553)/focal)+up*((334.5-pixel[1])/focal)
    return cam+ray*((z-cam.z)/ray.z)
source=unproject((270,416),125);target=unproject((982,153),150);distance=(target-source).length;length=distance*.40
verts=[];faces=[];uvs=[];slots=[];tags=[]
def patch(fn,nu,nv,slot,hot=False,reverse=True,tag=0.):
    first=len(verts)
    for i in range(nu+1):
        for j in range(nv+1):verts.append(tuple(fn(.0002+.9996*i/nu,j/nv)))
    for i in range(nu):
        for j in range(nv):
            x=first+i*(nv+1)+j;y=x+nv+1;faces.extend(((x,y,x+1),(x+1,y,y+1)));slots.extend((slot,slot));tags.extend((tag,tag))
            vv=lambda k:(1+math.cos(k/nv*math.tau))*.5
            u0=.99 if hot else i/nu;u1=.99 if hot else (i+1)/nu
            uvs.extend((((u0,vv(j)),(u1,vv(j)),(u0,vv(j+1))),((u0,vv(j+1)),(u1,vv(j)),(u1,vv(j+1)))))
            if reverse:
                faces[-2:]=[(a,c,b) for a,b,c in faces[-2:]]
                uvs[-2:]=[(a,c,b) for a,b,c in uvs[-2:]]
def silhouette(u):
    peak=1.1/(1.1+.62)
    return (u**1.1*(1-u)**.62)/(peak**1.1*(1-peak)**.62)

def core(u,v):
    w=length*.056*silhouette(u);phi=v*math.tau
    return Vector((-length*.82*(1-u),w*math.cos(phi),w*.85*math.sin(phi)))
patch(core,72,16,0)
def tip(u,v):
    theta=u*math.pi;phi=v*math.tau;r=length*.012
    return Vector((math.cos(theta)*r,math.sin(theta)*math.cos(phi)*r,math.sin(theta)*math.sin(phi)*r))
patch(tip,12,12,0,True,False)
def tip_glow(u,v):
    theta=u*math.pi;phi=v*math.tau
    return Vector((length*.026*math.cos(theta),length*.022*math.sin(theta)*math.cos(phi),length*.022*math.sin(theta)*math.sin(phi)))
patch(tip_glow,16,16,0,True,False,1.)
# 聚能细芒使用同一Core槽位；仅表现飞行尖端，不能冒充服务器确认命中。
for ray_axis in (1,2):
    def tip_ray(u,v):
        theta=u*math.pi;phi=v*math.tau
        p=Vector((length*.005*math.sin(theta)*math.cos(phi),length*.005*math.sin(theta)*math.sin(phi),length*.080*math.cos(theta)))
        if ray_axis==1:p.y,p.z=p.z,p.y
        return p
    patch(tip_ray,12,8,0,True,ray_axis==1,1.)
for index in range(3):
    def center(u):
        theta=index*math.tau/3+.4*(1-u);rad=length*.048*silhouette(u)
        return Vector((-length*(1-u),rad*math.cos(theta),rad*math.sin(theta)))
    data=bpy.data.curves.new('Editable_Petal_'+str(index),'CURVE');data.dimensions='3D';s=data.splines.new('BEZIER');s.bezier_points.add(12)
    for k,b in enumerate(s.bezier_points):b.co=center(k/12);b.handle_left_type='AUTO';b.handle_right_type='AUTO'
    curve=bpy.data.objects.new(data.name,data);scene.collection.objects.link(curve);curve.hide_set(True);curve.hide_render=True
    def petal(u,v):
        theta=index*math.tau/3+.4*(1-u);radial=Vector((0,math.cos(theta),math.sin(theta)));side=Vector((0,-math.sin(theta),math.cos(theta)));phi=v*math.tau
        width=length*.047*silhouette(u);thick=length*.019*silhouette(u)
        return center(u)+radial*width*math.cos(phi)+side*thick*math.sin(phi)
    patch(petal,72,16,1,tag=index/2)
def air(u,v):
    radius=length*.112*math.sin(math.pi*u)**.8;phi=v*math.tau
    return Vector((length*.04-length*1.14*(1-u),radius*math.cos(phi),radius*math.sin(phi)))
patch(air,48,8,2)
mesh=bpy.data.meshes.new('SM_DBA_DirectedMissile_V1');mesh.from_pydata(verts,[],faces);mesh.update();obj=bpy.data.objects.new(mesh.name,mesh);scene.collection.objects.link(obj)
for name,color in [('Core',(1,.55,.08,1)),('Petals',(1,.18,.012,1)),('AirWave',(.1,.1,.1,1))]:
    m=bpy.data.materials.new('M_Source_'+name);m.diffuse_color=color;mesh.materials.append(m)
uv=mesh.uv_layers.new(name='EnergyUV')
vc=mesh.color_attributes.new(name='EnergyLayer',type='FLOAT_COLOR',domain='CORNER');mesh.color_attributes.active_color=vc
for face,coords,slot,tag in zip(mesh.polygons,uvs,slots,tags):
    face.material_index=slot;face.use_smooth=True
    for i,xy in zip(face.loop_indices,coords):uv.data[i].uv=xy;vc.data[i].color=(tag,0.,0.,1.)
obj.select_set(True);bpy.context.view_layer.objects.active=obj
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_DBA_DirectedMissile_V1.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'DirectedMissileV1.blend'))
guide=dict(resolution=[1656,1002],referenceResolution=[1106,669],camera=list(cam),lookAt=list(look),fov=fov,source=list(source),target=list(target),casterFoot=list(unproject((150,560),0)),targetFoot=list(unproject((996,280),0)),direction=list((target-source).normalized()),distanceCm=distance,headLengthCm=length,flightDuration=.75,trailLifetime=1.,triangles=len(faces),vertices=len(verts),manualAccepted=False)
guide.update(styleRevision='translucent_energy_layers_03',headCoreRadiusRatio=.056,headOuterRadiusRatio=.095,petalHalfThicknessRatio=.019,tipGlowUsesCoreSlot=True,materialSlots=3,trailPhasesDegrees=[0,60,120])
(OUT/'ReferenceGuide.json').write_text(json.dumps(guide,indent=2),encoding='utf8');print('DIRECTED_MISSILE_SOURCE_SAVED',len(faces),distance)
