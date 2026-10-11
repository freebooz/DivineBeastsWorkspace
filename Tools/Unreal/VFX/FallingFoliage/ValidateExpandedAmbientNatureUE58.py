# -*- coding: utf-8 -*-
"""只读审查《神兽联盟》多地区39类自然飘落/漂浮VFX真实UE5.8资产合同。
只证明纹理、母材质、项目MI、Niagara渲染器、Definition及候选区域配置的存在和关联；
不以AssetRegistry加载成功冒充GPU编译、客户端Cook、区域激活或人工画面验收。
"""
from __future__ import annotations
import json
import hashlib
import unreal
from pathlib import Path

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
SRC=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
STYLE=json.loads((SRC/"AmbientNatureStyleCatalog.json").read_text(encoding="utf8"))
REGIONS=json.loads((SRC/"AmbientNatureAreaProfiles.json").read_text(encoding="utf8"))
MANIFEST=json.loads((SRC/"GeneratedTextureManifest.json").read_text(encoding="utf8"))
BASE="/DBAPresentationPack_Core/VFX/Environment/AmbientNature"
PLATFORM="/GamePlatformVFX/Environment/AmbientNature"
FALLING="/DBAPresentationPack_Core/VFX/Environment/FallingFoliage"
OUT=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary/UEAssetIntegrationAudit.json"
report={"engine":str(unreal.SystemLibrary.get_engine_version()),"scope":"ReadOnlyUEAssetValidation",
        "checkedStyles":0,"verifiedFallingVariants":0,"verifiedFloaterVariants":0,
        "verifiedWeatherDelegations":0,"regions":0,
        "errors":[],"warnings":[],"assetRows":[]}
styles={x["styleId"]:x for x in STYLE["styles"]}
if len(styles)!=len(STYLE["styles"]):report["errors"].append("StyleId重复")
if len(REGIONS["areas"])!=len({x["areaKey"] for x in REGIONS["areas"]}):report["errors"].append("区域Key重复")
for region in REGIONS["areas"]:
    pool=region["styles"]
    if not pool:
        report["errors"].append("区域效果池为空："+region["areaKey"])
        continue
    weight_sum=0.
    for ref in pool:
        if ref["styleId"] not in styles:
            report["errors"].append("区域引用缺失风格："+region["areaKey"]+"/"+ref["styleId"])
        weight_sum+=float(ref["weight"])
    if weight_sum<=0. or abs(weight_sum-1.)>.0001:
        report["errors"].append("区域风格权重之和应为1："+region["areaKey"])
report["regions"]=len(REGIONS["areas"])

source_specs={x["name"]:x for x in MANIFEST["textures"]}
if len(source_specs)!=len(MANIFEST["textures"]):report["errors"].append("源纹理清单文件名重复")
for style in STYLE["styles"]:
    key=style["styleId"]
    state=style["assetStatus"]
    if state=="delegated":
        if style["definitionId"]=="platform.weather.rain@1":
            path="/GamePlatformVFX/Weather/Definitions/DA_GP_VFX_Weather_Rain"
        elif style["definitionId"]=="platform.weather.snow@1":
            path="/GamePlatformVFX/Weather/Definitions/DA_GP_VFX_Weather_Snow"
        else:
            report["errors"].append("未知天气代理定义："+key)
            continue
        if not isinstance(unreal.load_asset(path),unreal.GamePlatformVFXAttachedDefinition):
            report["errors"].append("雨雪已有Definition不能加载："+path)
        else:report["verifiedWeatherDelegations"]+=1
        continue
    if state=="existing":
        path=FALLING+"/Definitions/DA_DBA_VFX_FallingFoliage_"+key
        if not isinstance(unreal.load_asset(path),unreal.GamePlatformVFXWorldDefinition):
            report["errors"].append("既有外观Definition不能加载："+path)
        continue
    if state!="planned":
        report["errors"].append("资产状态未知："+key)
        continue
    report["checkedStyles"]+=1
    row={"styleId":key,"category":style["category"],"assets":{}}
    for typ in ("Albedo","Normal"):
        name="T_DBA_Ambient_"+key+"_"+typ
        src=SRC/"Textures"/(name+"_Source.png")
        spec=source_specs.get(src.name)
        if not spec or not src.exists() or hashlib.sha256(src.read_bytes()).hexdigest()!=spec["sha256"]:
            report["errors"].append("源纹理哈希不一致："+src.name)
        path=BASE+"/Textures/"+name
        tex=unreal.load_asset(path)
        if not isinstance(tex,unreal.Texture2D):
            report["errors"].append("UE纹理资产缺失："+path)
        else:row["assets"][typ]=path
    mi_path=BASE+"/Materials/MI_DBA_Ambient_"+key
    mi=unreal.load_asset(mi_path)
    if not isinstance(mi,unreal.MaterialInstanceConstant):
        report["errors"].append("项目材质实例缺失："+mi_path)
        continue
    expected_parent={
        "petal":"/GamePlatformVFX/Environment/FallingFoliage/Materials/M_GP_FallingFoliage_Master",
        "leaf":"/GamePlatformVFX/Environment/FallingFoliage/Materials/M_GP_FallingFoliage_Master",
        "debris":"/GamePlatformVFX/Environment/FallingFoliage/Materials/M_GP_FallingFoliage_Master",
        "floater":PLATFORM+"/Materials/"+("M_GP_AmbientNature_Glow" if key in ("Ember","Firefly")
                   else "M_GP_AmbientNature_Soft")
    }[style["category"]]
    parent=mi.get_editor_property("parent")
    if not parent or parent.get_path_name().split(".")[0]!=expected_parent:
        report["errors"].append("项目材质父级不符："+mi_path)
    row["assets"]["material"]=mi_path
    ns_path=BASE+"/Niagara/NS_DBA_Ambient_"+key
    ns=unreal.load_asset(ns_path)
    if not isinstance(ns,unreal.NiagaraSystem):
        report["errors"].append("项目Niagara不存在："+ns_path)
        continue
    count=0
    expected_prefix=ns.get_path_name()+":"
    for renderer in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
        if renderer.get_path_name().startswith(expected_prefix):
            count+=1
            actual=renderer.get_editor_property("material")
            if actual is None or actual.get_path_name()!=mi.get_path_name():
                report["errors"].append("NiagaraRenderer材质绑定错误："+ns_path)
    if count==0:
        report["errors"].append("没有合法NiagaraSpriteRenderer："+ns_path)
    else:
        if style["category"]=="floater":report["verifiedFloaterVariants"]+=1
        else:report["verifiedFallingVariants"]+=1
    row["assets"]["niagara"]=ns_path
    definition_path=BASE+"/Definitions/DA_DBA_VFX_Ambient_"+key
    definition=unreal.load_asset(definition_path)
    if not isinstance(definition,unreal.GamePlatformVFXWorldDefinition):
        report["errors"].append("Definition类型/位置错误："+definition_path)
    else:
        system_ref=definition.get_editor_property("niagara_system")
        system_path=system_ref.get_path_name().split(".")[0] if hasattr(system_ref,"get_path_name") else str(system_ref)
        if ns_path not in system_path:
            report["errors"].append("Definition没有绑定目标Niagara："+definition_path)
        row["assets"]["definition"]=definition_path
    report["assetRows"].append(row)

report["requiredRendererVariants"]=len([x for x in STYLE["styles"] if x["assetStatus"]=="planned"])
report["errorsCount"]=len(report["errors"])
report["passed"]=len(report["errors"])==0
if STYLE.get("sourceImageTransferStatus")!="generated_conversation_images_imported":
    report["warnings"].append("对话生成的高品质图片原件未进入Runner；当前UE材质使用程序生成的独立来源")
report["warnings"].append("自动校验不评估飘浮微粒专属运动（火星升力、萤火虫趋光）、实际客户端画面或Cook/Stage")
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
print("AMBIENT_UE_ASSET_AUDIT",report["passed"],
      "styles",report["checkedStyles"],"falling",report["verifiedFallingVariants"],
      "floaters",report["verifiedFloaterVariants"],
      "weather",report["verifiedWeatherDelegations"],
      "regions",report["regions"],"errors",report["errorsCount"])
for err in report["errors"]:print("AMBIENT_UE_ASSET_ERROR",err)
