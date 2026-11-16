# -*- coding: utf-8 -*-
"""独立重开攻击弧刃V2的.blend，验证可编辑曲线、三维网格/UV及多材质源合同。

由Blender后台执行；此测试不代替UE渲染或人工审核。
"""
import json
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V2/AttackSlash'
BLEND=OUT/'AttackSlashV2.blend'
assert BLEND.is_file(),'MISSING_SAVED_BLEND: 先制作后必须独立重开验证'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.open_mainfile(filepath=str(BLEND))
mesh=bpy.data.objects.get('SM_DBA_AttackSlash_V2')
assert mesh and mesh.type=='MESH','缺少唯一多材质输出网格'
assert len(mesh.data.materials)==3,'主体/流带/金色沿边必须是三个明确材质槽'
assert len(mesh.data.uv_layers)>0,'缺少沿曲线UV'
assert 1000<len(mesh.data.polygons)<24000,'几何量超出本样板合理范围'
curves=[o for o in bpy.data.objects if o.type=='CURVE']
assert len(curves)>=8,'缺少可编辑设计曲线'
assert all(s.type=='BEZIER' for o in curves for s in o.data.splines),'曲线源未保留Bezier'
positions=[v.co for v in mesh.data.vertices]
extents=[max(p[i] for p in positions)-min(p[i] for p in positions) for i in range(3)]
assert extents[0]>450 and extents[1]>400 and extents[2]>50,'主刃面未形成参考图要求的空间尺度与体积'
# 排除倾斜平面伪造体积：还原设计平面后检查宽刃自身的真实凸面厚度。
body_indices={i for p in mesh.data.polygons if p.material_index==0 for i in p.vertices}
thickness=[positions[i].z+.08*positions[i].y-100 for i in body_indices]
assert max(thickness)-min(thickness)>28,'宽刃主体必须有真实厚度，不能以整片倾斜代替'
assert all(v.normal.length>.99 for v in mesh.data.vertices),'顶点法线无效'
assert (OUT/'SM_DBA_AttackSlash_V2.fbx').is_file(),'缺少可导入FBX'
for name in ('T_DBA_SlashV2_Color.png','T_DBA_SlashV2_Masks.png','T_DBA_SlashV2_Normal.png'):
    assert (OUT/name).is_file(),'缺少职责贴图：'+name
report=dict(status='passed',blend=str(BLEND),editableBezierCurves=len(curves),vertices=len(mesh.data.vertices),polygons=len(mesh.data.polygons),materialSlots=3,extentsCm=extents,manualAccepted=False)
path=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2/BlendValidation.json';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(report,indent=2),encoding='utf8')
print('ATTACK_SLASH_V2_SAVED_BLEND_VALID',json.dumps(report))
