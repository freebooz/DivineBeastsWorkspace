# -*- coding:utf-8 -*-
"""UE5.8只读验证通用Niagara PBR母材质，不修改任何二进制资源。
确认真实双面植被着色模型、独立Opacity、Normal、Roughness和Subsurface相关参数存在，
并且平台母材质未依赖神兽联盟专用项目纹理。
"""
from __future__ import annotations
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve().parent
path="/GamePlatformVFX/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage"
out=root/"Saved/Validation/FallingFoliage/PBRAtlas20261011/PBRMasterValidation.json"
report={"path":path,"pass":False,"errors":[],"nodes":[]}
material=unreal.EditorAssetLibrary.load_asset(path)
if not isinstance(material,unreal.Material):
    report["errors"].append("通用PBR母材质不存在")
else:
    props={}
    for prop in ("two_sided","used_with_niagara_sprites","blend_mode","shading_model","opacity_mask_clip_value"):
        try:props[prop]=str(material.get_editor_property(prop))
        except Exception as e:report["errors"].append("无法读取Material属性："+prop+": "+str(e))
    report["properties"]=props
    if props.get("two_sided")!="True":report["errors"].append("未开启双面渲染")
    if props.get("used_with_niagara_sprites")!="True":report["errors"].append("未开启NiagaraSprite资源使用")
    if "MASKED" not in props.get("blend_mode","").upper():report["errors"].append("必须使用透明裁剪")
    if "FOLIAGE" not in props.get("shading_model","").upper():report["errors"].append("必须使用受光的双面植被着色")
    expressions=unreal.MaterialEditingLibrary.get_material_expressions(material)
    for node in expressions:
        data={"type":node.get_class().get_name(),"name":node.get_name()}
        try:data["parameter"]=str(node.get_editor_property("parameter_name"))
        except Exception:pass
        report["nodes"].append(data)
    names={x.get("parameter","") for x in report["nodes"]}
    required=("LeafAlbedo","LeafNormal","LeafOpacity","LeafRoughnessMap","RoughnessScale","OpacityMultiplier","LeafSpecular","LeafTransmission")
    for name in required:
        if name not in names:report["errors"].append("缺少稳定材质参数："+name)
    if not any(x["type"]=="MaterialExpressionParticleColor" for x in report["nodes"]):
        report["errors"].append("缺少与Niagara ParticleColor的统一调色输入")
    for node in report["nodes"]:
        if any("/DBA" in str(value) for value in node.values()):
            report["errors"].append("平台材质出现项目层硬依赖："+node["name"])
report["pass"]=not report["errors"]
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
print("AMBIENT_PBR_MASTER_ASSET_VALIDATION",report["pass"],"nodes",len(report["nodes"]),"errors",len(report["errors"]),flush=True)
for item in report["errors"]:print("AMBIENT_PBR_MASTER_ASSET_ERROR",item,flush=True)
