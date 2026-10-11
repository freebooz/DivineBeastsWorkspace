#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟自然环境资源库静态验收（不需要启动UE编辑器）。
校验39风格、21区域、33套源PNG/UE资产及按端Cook规则；
不能替代Niagara GPU与地图人工验收。
"""
from __future__ import annotations
import json
import hashlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core"
SOURCE=BASE/"SourceArt/AmbientNature"
ASSET=BASE/"Content/VFX/Environment/AmbientNature"
PLATFORM=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX"
REPORT=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary/StaticAssetLibraryAudit.json"
issues=[]
checked=0

def check(ok:bool,message:str):
    global checked
    checked+=1
    if not ok:issues.append(message)

styles=json.loads((SOURCE/"AmbientNatureStyleCatalog.json").read_text(encoding="utf-8"))["styles"]
areas=json.loads((SOURCE/"AmbientNatureAreaProfiles.json").read_text(encoding="utf-8"))["areas"]
manifest=json.loads((SOURCE/"GeneratedTextureManifest.json").read_text(encoding="utf-8"))["textures"]
unique={s["styleId"] for s in styles}
check(len(styles)==39 and len(unique)==39,"风格清单必须为39项且不重复")
check(len(areas)==21 and len({a["areaKey"] for a in areas})==21,"区域清单必须为21项且不重复")
planned=[s for s in styles if s["assetStatus"]=="planned"]
delegated=[s for s in styles if s["assetStatus"]=="delegated"]
existing=[s for s in styles if s["assetStatus"]=="existing"]
check(len(planned)==33 and len(delegated)==2 and len(existing)==4,"新增/天气代理/已有风格数量不符")
check(len(manifest)==66 and len({x["name"] for x in manifest})==66,"源纹理文件清单不应重复且有66份")
for style in planned:
    name=style["styleId"]
    for part in ("Albedo","Normal"):
        source=SOURCE/"Textures"/f"T_DBA_Ambient_{name}_{part}_Source.png"
        entry=next((t for t in manifest if t["name"]==source.name),None)
        check(bool(entry) and source.is_file() and
              hashlib.sha256(source.read_bytes()).hexdigest()==entry["sha256"],
              f"源纹理SHA不符：{name}/{part}")
        asset=ASSET/"Textures"/f"T_DBA_Ambient_{name}_{part}.uasset"
        check(asset.is_file() and asset.stat().st_size>128,
              f"UE纹理文件缺失：{asset.name}")
    for category,prefix in (("Materials","MI_DBA_Ambient_"),
                            ("Niagara","NS_DBA_Ambient_"),
                            ("Definitions","DA_DBA_VFX_Ambient_")):
        file=ASSET/category/(prefix+name+".uasset")
        check(file.is_file() and file.stat().st_size>128,
              "UE资产不存在："+str(file))
for style in existing:
    name=style["styleId"]
    path=BASE/"Content/VFX/Environment/FallingFoliage/Definitions"/f"DA_DBA_VFX_FallingFoliage_{name}.uasset"
    check(path.is_file(),"既有飘落定义缺失："+name)
for file in (
        PLATFORM/"Content/Environment/AmbientNature/Materials/M_GP_AmbientNature_Soft.uasset",
        PLATFORM/"Content/Environment/AmbientNature/Materials/M_GP_AmbientNature_Glow.uasset",
        PLATFORM/"Content/Environment/AmbientNature/Niagara/NS_GP_AmbientFloat_Base.uasset"):
    check(file.is_file() and file.stat().st_size>128,"平台共用资源缺失："+str(file))
for weather in ("Rain","Snow"):
    weather_file=PLATFORM/"Content/Weather/Definitions"/f"DA_GP_VFX_Weather_{weather}.uasset"
    check(weather_file.is_file(),"天气专属定义必须复用："+weather)
for area in areas:
    check(bool(area["styles"]) and
          abs(sum(float(x["weight"]) for x in area["styles"])-1.0)<0.0001 and
          all(x["styleId"] in unique for x in area["styles"]),
          "区域样式权重或风格ID错误："+area["areaKey"])

engine=(ROOT/"Game/Config/DefaultGame.ini").read_text(encoding="utf-8")
check('/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Definitions' in engine,
      "主资产扫描目录缺少新增Definition")
for path in ("Game/Config/Custom/FrontEndClient/DefaultGame.ini",
             "Game/Config/Custom/VillageDevelopmentClient/DefaultGame.ini"):
    content=(ROOT/path).read_text(encoding="utf-8")
    check('/DBAPresentationPack_Core/VFX/Environment/AmbientNature' in content,
          "客户端缺少项目AmbientNature资源Cook目录："+path)
    check('/GamePlatformVFX/Environment/AmbientNature' in content,
          "客户端缺少平台AmbientNature资源Cook目录："+path)
server=(ROOT/"Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini").read_text(encoding="utf-8")
check('/DBAPresentationPack_Core/...' in server and '/GamePlatformVFX/...' in server,
      "专用服务器必须剥离全部表现资源")
check((ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Public/Ambient/DivineBeastsAmbientRegionProfile.h").is_file(),
      "缺少区域预设公开类型")
header=(ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Public/Ambient/DivineBeastsAmbientRegionProfile.h").read_text(encoding="utf-8")
check("FName DefinitionId" in header and "SelectDefinitionId" in header,
      "区域风格权重必须持有Definition身份且暴露同权重选择API")
author=(ROOT/"Tools/Unreal/VFX/FallingFoliage/AuthorAmbientRegionProfilesUE58.py").read_text(encoding="utf-8")
check('data.set_editor_property("definition_id"' in author,
      "区域真实DataAsset生成器必须写入DefinitionId，拒绝只存StyleId")
check((ROOT/"Tools/Unreal/VFX/FallingFoliage/AuthorAmbientRegionProfilesUE58.py").is_file(),
      "缺少真实UE区域预设DataAsset作者工具")
check((ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationRuntime/Private/Tests/DivineBeastsAmbientRegionProfileTests.cpp").is_file(),
      "缺少区域风格权重选择自动化源码")

REPORT.parent.mkdir(parents=True,exist_ok=True)
result={"passed":not issues,"checks":checked,"failed":len(issues),
        "styles":len(styles),"newStyles":len(planned),"zones":len(areas),
        "warnings":["静态文件校验不证明Niagara实际运动、角色/区域已激活、目标设备Cook/Stage与性能",
                    "来自本对话的5张高品质图集尚未通过连接器原图传输，正式美术替换未完成"],
        "errors":issues}
REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
print("AMBIENT_LIBRARY_STATIC_AUDIT",result["passed"],
      "checks",checked,"errors",len(issues),
      "styles",len(styles),"areas",len(areas))
for i in issues:print("AMBIENT_STATIC_ERROR",i)
raise SystemExit(0 if not issues else 1)
