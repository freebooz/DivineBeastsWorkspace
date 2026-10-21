# -*- coding: utf-8 -*-
"""SSLM戌狗V6原生48像素Texture2D导出后的像素颜色核验。

UE5.8命令行Python真实加载已有八张uasset，通过编辑器内置TextureExporterPNG
输出审核PNG，再与本次源PNG逐像素比较。仅向Saved写审核材料，不修改技能/
Profile/地图；技术差异不等于用户的主观艺术通过。
"""
from pathlib import Path
import sys,json,hashlib
import unreal
ROOT=Path(__file__).resolve().parents[3]
PACK=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog"
DATA=json.loads((PACK/"Docs/DogV6_DerivedArtManifest_20261020.json").read_text("utf-8"))
OUT=ROOT/"Saved/Validation/DogV6Staging20261019/EnginePixelExport"
OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/"Saved/Tools/SnowPyDeps"))
from PIL import Image, ImageChops, ImageStat
reg=unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/DBAHeroPack_Dog/UI/Abilities"],force_rescan=True)
lib=unreal.EditorAssetLibrary
results=[]
for record in DATA["assets"]:
    p=record["texturePath"]
    name=record["textureAssetName"]
    asset=lib.load_asset(p)
    if not isinstance(asset,unreal.Texture2D):
        raise RuntimeError("真实Texture2D丢失："+p)
    f=OUT/(name+"_Export48.png")
    if f.exists():
        raise RuntimeError("已存在相同的审核导出文件，请不要覆盖："+str(f))
    task=unreal.AssetExportTask()
    task.set_editor_property("object",asset)
    task.set_editor_property("filename",str(f))
    task.set_editor_property("automated",True)
    task.set_editor_property("prompt",False)
    task.set_editor_property("replace_identical",False)
    if hasattr(unreal,"TextureExporterPNG"):
        task.set_editor_property("exporter",unreal.TextureExporterPNG())
    ok=unreal.Exporter.run_asset_export_task(task)
    if not ok or not f.is_file():
        raise RuntimeError("引擎原生导出失败："+p)
    source=ROOT/record["files"]["48"]["path"]
    if hashlib.sha256(source.read_bytes()).hexdigest()!=record["files"]["48"]["sha256"]:
        raise RuntimeError("源PNG哈希不一致："+str(source))
    with Image.open(source) as src, Image.open(f) as rendered:
        a=src.convert("RGB");b=rendered.convert("RGB")
        if a.size!=(48,48) or b.size!=(48,48):
            raise RuntimeError("导出图不是真实48像素："+name)
        diff=ImageChops.difference(a,b)
        extrema=diff.getextrema()
        maximum=max(hi for lo,hi in extrema)
        mismatch=sum(1 for px in diff.getdata() if max(px)>2)
        mean=[round(x,3) for x in ImageStat.Stat(diff).mean]
    results.append({"asset":p,"source48SHA256":record["files"]["48"]["sha256"],
                    "export48SHA256":hashlib.sha256(f.read_bytes()).hexdigest(),
                    "maxChannelDelta":maximum,"pixelsWithDeltaOver2":mismatch,
                    "absoluteRGBMean":mean,"isVisuallyExact":maximum<=2 and mismatch==0})
report={"heroId":"Hero.Zodiac.Dog","engineVersion":unreal.SystemLibrary.get_engine_version(),
        "ueNativeExportCount":len(results),"technicalRGBPass":all(r["isVisuallyExact"] for r in results),
        "caveatZh":"仅对UE当前SourceArt导出路径做原生PNG与源PNG像素比较；最终HUD经过Slate采样和DPI仍需实机截图。",
        "assets":results}
(OUT/"DogV6_EnginePixelAudit.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n","utf-8")
print(json.dumps({"count":len(results),"pass":report["technicalRGBPass"],
                  "maxChannelDelta":max(r["maxChannelDelta"] for r in results),
                  "report":str(OUT/"DogV6_EnginePixelAudit.json")},ensure_ascii=False))
if not report["technicalRGBPass"]:
    raise RuntimeError("原生纹理导出颜色与PNG源不完全一致，需要审查具体报告")
