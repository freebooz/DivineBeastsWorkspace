# -*- coding: utf-8 -*-
"""按凝锋参考镜头重建空间翻卷刀翼、薄流膜和细锋，可编辑Bezier源留在项目第三层。

屏幕引导点只约束轮廓；不同深度、真实凸面厚度、分离流膜组成可从侧面检查的三维网格。
"""
from pathlib import Path
from datetime import datetime,timezone
import argparse,json,math,shutil,sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash'
parser=argparse.ArgumentParser();parser.add_argument('--revise-own-output',action='store_true');options=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
OUT.mkdir(parents=True,exist_ok=True)
if (OUT/'AttackSlashV3.blend').exists():
    assert options.revise_own_output,'拒绝覆盖已有源'
    rev=OUT/'Revisions'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ');rev.mkdir(parents=True)
    for n in ('AttackSlashV3.blend','SM_DBA_AttackSlash_V3.fbx','ReferenceGuide.json'):shutil.copy2(OUT/n,rev/n)
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01;scene.world=bpy.data.worlds.new('SourceWorld')
controls=bpy.data.collections.new('Editable_Bezier_Design');scene.collection.children.link(controls);controls.hide_render=True
runtime=bpy.data.collections.new('Runtime_Mesh');scene.collection.children.link(runtime)
CAM=Vector((0,-650,270));LOOK=Vector((0,0,120));FWD=(LOOK-CAM).normalized();RIGHT=Vector((0,0,1)).cross(FWD).normalized();UP=FWD.cross(RIGHT).normalized();FOCAL=1536/(2*math.tan(math.radians(60)/2));ORIGIN=Vector((195,-179,0))
OUTER=[(15,777),(210,817),(530,745),(870,583),(1128,358),(1244,183),(1142,96),(912,74),(700,94)]
INNER=[(15,777),(235,745),(545,616),(814,443),(1007,267),(1065,160),(999,120),(857,100),(700,94)]
DEPTH=[400,440,490,530,570,610,630,650,650]
def unproject(p,d):
    world=CAM+FWD*d+RIGHT*((p[0]-768)*d/FOCAL)+UP*((512-p[1])*d/FOCAL)-ORIGIN
    return Vector((world.x,-world.y,world.z))
outer=[unproject(p,d) for p,d in zip(OUTER,DEPTH)];inner=[unproject(p,d-6*math.sin(math.pi*i/8)) for i,(p,d) in enumerate(zip(INNER,DEPTH))]
# Inner的深度变化是空间层次，投影控制点仍保持完全一致。
def sample(points,t):
    f=min(.999999,max(0,t))*(len(points)-1);i=int(f);q=f-i
    a=points[max(0,i-1)];b=points[i];c=points[min(len(points)-1,i+1)];d=points[min(len(points)-1,i+2)]
    return .5*((2*b)+(-a+c)*q+(2*a-5*b+4*c-d)*q*q+(-a+3*b-3*c+d)*q*q*q)
def curve(name,points):
    data=bpy.data.curves.new(name,'CURVE');data.dimensions='3D';data.resolution_u=24;s=data.splines.new('BEZIER');s.bezier_points.add(len(points)-1)
    for i,(b,p) in enumerate(zip(s.bezier_points,points)):
        delta=(points[min(len(points)-1,i+1)]-points[max(0,i-1)])/6;b.co=p;b.handle_left_type='FREE';b.handle_right_type='FREE';b.handle_left=p-delta;b.handle_right=p+delta
    obj=bpy.data.objects.new(name,data);controls.objects.link(obj);obj.hide_set(True)
def frame(u):
    o=sample(outer,u);i=sample(inner,u);w=o-i;center=(o+i)*.5
    tangent=(sample(outer,min(1,u+.0001))+sample(inner,min(1,u+.0001))-sample(outer,max(0,u-.0001))-sample(inner,max(0,u-.0001))).normalized()
    normal=tangent.cross(w).normalized();return i,o,center,normal
def sheet(u,v,height=0):
    i,o,c,n=frame(u);return i+(o-i)*v+n*height
verts=[];faces=[];uvs=[];slots=[];groups={}
def patch(fn,nu,nv,slot,group=None):
    offset=len(verts)
    for a in range(nu+1):
        for b in range(nv+1):verts.append(tuple(fn(.0002+.9996*a/nu,b/nv)))
    for a in range(nu):
        for b in range(nv):
            x=offset+a*(nv+1)+b;y=x+nv+1;faces.extend(((x,y,x+1),(x+1,y,y+1)))
            vv=lambda k:(1+math.cos(k/nv*math.tau))*.5
            uvs.extend((((a/nu,vv(b)),((a+1)/nu,vv(b)),(a/nu,vv(b+1))),((a/nu,vv(b+1)),((a+1)/nu,vv(b)),((a+1)/nu,vv(b+1)))))
            slots.extend((slot,slot))
    if group:groups[group]=list(range(offset,len(verts)))
curve('Shape_Outer',outer);curve('Shape_Inner',inner)
def blade(u,v):
    i,o,c,n=frame(u);phi=v*math.tau
    # 前后凸面的厚度及翻卷在世界空间成立，而非只在镜头里画一块平面。
    return c+(o-i)*.5*math.cos(phi)+n*(2+6*math.sin(math.pi*u))*math.sin(phi)
patch(blade,160,16,0)
def ribbon(name,points,width,slot,flatten=.12):
    curve(name,points)
    def fn(u,v):
        p=sample(points,u);t=(sample(points,min(1,u+.0002))-sample(points,max(0,u-.0002))).normalized()
        n=Vector((.1,-.8,.6));side=t.cross(n).normalized();n=side.cross(t).normalized()
        radius=max(.004,width*math.sin(math.pi*u)**.8)
        if name.startswith('Cloud'):radius*=.25+1.1*math.sin(math.pi*u)**2
        return p+side*math.cos(v*math.tau)*radius+n*math.sin(v*math.tau)*radius*flatten
    patch(fn,100 if name.startswith('Cloud') else 96,6 if slot==1 else 4,slot)
# 两条低位薄流膜只承接挥斩，顶部由宽幅火焰体承担，不再铺满平行长丝。
for j,(v,width,height) in enumerate(((.84,3.6,5),(.30,4.6,-9))):
    pts=[sheet(k/12,v+.13*math.sin(k/12*math.pi*2+j),height*math.sin(math.pi*k/12)) for k in range(13)]
    ribbon('FlowMembrane_'+str(j),pts,width,1,.07)
# 只留两条低位内嵌软流痕，移除顶部游丝及放射短丝，避免亚像素线条和断续亮点。
for j,v in enumerate((.82,.25)):
    pts=[sheet(.02+k/14*.48,v+.04*math.sin(k/14*math.pi*2+j),2*math.sin(math.pi*k/14)) for k in range(15)]
    ribbon('Filament_'+str(j),pts,.35,2,.35)

def flame_fold(name,edge_a,edge_b,depths,offset,thickness):
    """以两条独立轮廓形成宽幅卷瓣及闭合透镜截面，宽度/厚度直接写入真实网格。"""
    a=[unproject(p,d+offset) for p,d in zip(edge_a,depths)]
    b=[unproject(p,d+offset) for p,d in zip(edge_b,depths)]
    curve(name+'_Outer',a);curve(name+'_Inner',b)
    def fn(u,v):
        pa=sample(a,u);pb=sample(b,u);center=(pa+pb)*.5
        tangent=(sample(a,min(1,u+.0002))+sample(b,min(1,u+.0002))-sample(a,max(0,u-.0002))-sample(b,max(0,u-.0002))).normalized()
        normal=tangent.cross(pa-pb).normalized();angle=v*math.tau
        # 卷瓣深度和局部厚度分开；中心抬起形成前后交错，不靠共面透明片叠加造厚。
        bulge=math.sin(math.pi*u)**.8
        return center+(pa-pb)*.5*math.cos(angle)+normal*(thickness*bulge*math.sin(angle)+8*math.sin(math.pi*u)*math.sin(u*math.pi*1.5))
    patch(fn,80,12,1,name)

flame_fold('FlameFold_Front',
 [(630,605),(870,430),(1000,250),(1035,150),(970,105),(905,92)],
 [(630,605),(760,447),(880,280),(920,180),(934,126),(905,92)],
 [525,565,610,630,646,651],-24,3.8)
flame_fold('FlameFold_Middle',
 [(795,522),(1020,360),(1150,216),(1115,133),(1010,93)],
 [(795,522),(898,354),(1025,219),(1032,153),(1010,93)],
 [543,582,615,640,653],8,5.0)
flame_fold('FlameFold_Back',
 [(920,420),(1125,300),(1224,198),(1180,120),(1080,89)],
 [(920,420),(1020,293),(1119,187),(1129,134),(1080,89)],
 [563,598,621,643,654],30,6.0)

# 同一Mesh/粒子中的有界空气包络，略宽于火焰主体；只承担局部折射，不生成新System。
def air_shell(u,v):
    i,o,c,n=frame(u);angle=v*math.tau
    return c+(o-i)*.61*math.cos(angle)+n*((12+4*math.sin(math.pi*u))*math.sin(angle)+8)
patch(air_shell,96,8,3,'AirWaveEnvelope')
data=bpy.data.meshes.new('SM_DBA_AttackSlash_V3');data.from_pydata(verts,[],faces);data.update();obj=bpy.data.objects.new('SM_DBA_AttackSlash_V3',data);runtime.objects.link(obj)
for name,indices in groups.items():obj.vertex_groups.new(name=name).add(indices,1.,'REPLACE')
for name,col in [('Body',(1,.06,.008)),('FlowSheets',(1,.3,.035)),('Filaments',(1,.65,.2)),('AirWave',(.1,.1,.1))]:
    mat=bpy.data.materials.new('M_Source_'+name);mat.diffuse_color=(*col,1);data.materials.append(mat)
layer=data.uv_layers.new(name='FlowUV')
for p,coords,slot in zip(data.polygons,uvs,slots):
    p.material_index=slot;p.use_smooth=True
    for index,uv in zip(p.loop_indices,coords):layer.data[index].uv=uv
obj.select_set(True);bpy.context.view_layer.objects.active=obj
obj['Reference']='Reference_凝锋.png';obj['ManualAccepted']=False
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_DBA_AttackSlash_V3.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AttackSlashV3.blend'))
guide=dict(resolution=[1536,1024],camera=list(CAM),lookAt=list(LOOK),fov=60,effectOrigin=list(ORIGIN),outerPixels=OUTER,innerPixels=INNER,depthCm=DEPTH,filamentHalfWidthMaxCm=.35,vertices=len(verts),triangles=len(faces),materialSlots=4,styleRevision='broad_flame_air_02',broadFlameFoldCount=3,lowEmbeddedStrands=2,contactStrands=0,airWaveEnvelope=True,manualAccepted=False)
(OUT/'ReferenceGuide.json').write_text(json.dumps(guide,indent=2),encoding='utf8');print('ATTACK_SLASH_V3_SOURCE_SAVED',len(faces),'triangles')
