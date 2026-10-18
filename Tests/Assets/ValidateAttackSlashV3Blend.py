# -*- coding: utf-8 -*-
"""独立重开凝锋V3源，验证空间翻卷、细锋截面及与参考镜头的投影控制点。"""
from pathlib import Path
import json,math
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash'
assert (SOURCE/'AttackSlashV3.blend').is_file(),'MISSING_V3_SAVED_BLEND'
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'AttackSlashV3.blend'))
guide=json.loads((SOURCE/'ReferenceGuide.json').read_text(encoding='utf8'))
camera=Vector(guide['camera']);forward=(Vector(guide['lookAt'])-camera).normalized();right=Vector((0,0,1)).cross(forward).normalized();up=forward.cross(right).normalized();focal=guide['resolution'][0]/(2*math.tan(math.radians(guide['fov'])/2))
root=Vector(guide['effectOrigin']);max_error=0
for name,key in [('Shape_Outer','outerPixels'),('Shape_Inner','innerPixels')]:
    curve=bpy.data.objects[name];points=curve.data.splines[0].bezier_points;assert len(points)==len(guide[key])
    for p,expected in zip(points,guide[key]):
        world=Vector((p.co.x,-p.co.y,p.co.z))+root;offset=world-camera;depth=offset.dot(forward)
        x=768+offset.dot(right)*focal/depth;y=512-offset.dot(up)*focal/depth
        error=math.hypot(x-expected[0],y-expected[1]);max_error=max(error,max_error);assert error<.02,'参考轮廓控制点投影不符'
mesh=bpy.data.objects['SM_DBA_AttackSlash_V3'];assert mesh.type=='MESH' and len(mesh.data.materials)==3
assert 4000<len(mesh.data.polygons)<24000
assert len([o for o in bpy.data.objects if o.type=='CURVE'])>=16
assert len(mesh.data.uv_layers)==1 and all(v.normal.length>.99 for v in mesh.data.vertices)
positions=[v.co for v in mesh.data.vertices];span=[max(v[i] for v in positions)-min(v[i] for v in positions) for i in range(3)]
assert span[2]>230,'必须是从低处挑起的空间翻卷，不能是水平月牙'
assert guide['filamentHalfWidthMaxCm']<=.5,'亮锋不能退化成粗实管线'
assert (SOURCE/'SM_DBA_AttackSlash_V3.fbx').is_file()
out=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3';out.mkdir(parents=True,exist_ok=True)
report=dict(passed=True,projectedGuideMaxErrorPixels=max_error,vertices=len(mesh.data.vertices),triangles=len(mesh.data.polygons),materialSlots=3,extentCm=span,filamentHalfWidthMaxCm=guide['filamentHalfWidthMaxCm'],manualAccepted=False)
(out/'BlendValidation.json').write_text(json.dumps(report,indent=2),encoding='utf8');print('ATTACK_SLASH_V3_SAVED_BLEND_VALID',json.dumps(report))
