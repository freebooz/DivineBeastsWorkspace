# -*- coding: utf-8 -*-
"""空白会话重开定向飞弹源，核对可编辑立体弹头、槽位与参考构图合同。"""
from pathlib import Path
import json,math
import bpy
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile'
assert (SOURCE/'DirectedMissileV1.blend').exists(),'MISSING_DIRECTED_MISSILE_SAVED_SOURCE'
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'DirectedMissileV1.blend'))
obj=bpy.data.objects['SM_DBA_DirectedMissile_V1'];assert obj.type=='MESH' and len(obj.data.materials)==3
assert len(obj.data.uv_layers)==1 and all(v.normal.length>.99 for v in obj.data.vertices)
assert len([o for o in bpy.data.objects if o.type=='CURVE'])==3
p=[v.co for v in obj.data.vertices];span=[max(v[i] for v in p)-min(v[i] for v in p) for i in range(3)]
assert span[0]>span[1]*2 and min(span[1:])>15,'弹头须是沿前向的立体梭形，不能用平面截图代替'
volumes=[0.,0.,0.]
for face in obj.data.polygons:
    a,b,c=[obj.data.vertices[i].co for i in face.vertices]
    volumes[face.material_index]+=a.dot(b.cross(c))/6
assert all(v>1 for v in volumes),'近侧外壳/空气包络须朝外，不能通过双面材质掩盖倒面：'+str(volumes)
guide=json.loads((SOURCE/'ReferenceGuide.json').read_text(encoding='utf8'));c=Vector(guide['camera']);f=(Vector(guide['lookAt'])-c).normalized();r=Vector((0,0,1)).cross(f).normalized();u=f.cross(r).normalized();focal=1106/(2*math.tan(math.radians(guide['fov'])/2))
errors=[]
for name,pixel in [('source',[270,416]),('target',[982,153])]:
    v=Vector(guide[name])-c;d=v.dot(f);actual=[553+v.dot(r)*focal/d,334.5-v.dot(u)*focal/d];errors.append(math.dist(actual,pixel))
assert max(errors)<.05
out=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1';out.mkdir(parents=True,exist_ok=True)
report=dict(passed=True,triangles=len(obj.data.polygons),vertices=len(obj.data.vertices),materialSlots=3,extentCm=span,signedVolumesCm3=volumes,guideProjectionErrorPixels=max(errors),manualAccepted=False)
(out/'BlendValidation.json').write_text(json.dumps(report,indent=2),encoding='utf8');print('DIRECTED_MISSILE_SOURCE_VALID',json.dumps(report))
