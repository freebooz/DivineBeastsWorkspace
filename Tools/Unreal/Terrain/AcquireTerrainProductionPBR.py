#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""十层正式PBR同源扫描纹理制作：暂存->官方签名验证->备份->覆盖原路径。

数据层：仅操作项目DBAWorldPack_Village的PBR SourceArt和来源清单；
不直接写UE二进制资产、地图、权重、Gaea高程或PCG图。UE重导入另行验证。
来源Poly Haven官网（CC0），同一地表的BaseColor/NormalDX/ORM/Height必须同源。
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import shutil
import time
from urllib.request import Request, urlopen
import zipfile
from PIL import Image, ImageChops

ROOT=Path(__file__).resolve().parents[3]
TOOL=ROOT/"Tools/Unreal/Terrain"
WORLD=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village"
PBR=WORLD/"SourceArt/Terrain/PBR"
TEX=WORLD/"Content/Terrain/Textures"
OUT=ROOT/"Game/Saved/Validation/TerrainPBR_20261012"
STAGE=OUT/"Stage"
OLD=TOOL/"TerrainSourceManifest_v1.json"
PROD=TOOL/"TerrainProductionPBRManifest_v1.json"
PLAN=OUT/"ProductionStageManifest.json"
SOURCES={
 "Grass":"leafy_grass","ForestSoil":"forest_ground_05","Dirt":"park_dirt",
 "Rock":"gray_rocks","Scree":"rocky_terrain_03","Sand":"damp_beach_sand",
 "Gravel":"gravel_ground_01","RoadDirt":"stony_dirt_path",
 "FarmSoil":"farm_furrows","ShoreMud":"brown_mud_03"}
ROLES={"BaseColor":"Diffuse","Normal":"nor_dx","ORM":"arm","Height":"Displacement"}
COMP={"BaseColor":"Default","Normal":"Normalmap","ORM":"Masks","Height":"Grayscale"}

def digest(path,algorithm="sha256"):
 h=hashlib.new(algorithm)
 with path.open("rb") as inp:
  for part in iter(lambda:inp.read(1024*1024),b""):h.update(part)
 return h.hexdigest()

def read_json(path):
 return json.loads(path.read_text(encoding="utf-8-sig"))

def write_json(path,data):
 path.parent.mkdir(parents=True,exist_ok=True)
 temp=path.with_suffix(path.suffix+".new")
 temp.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
 temp.replace(path)

def fetch(url,path,size=0,md5=""):
 if not url.startswith(("https://api.polyhaven.com/","https://dl.polyhaven.org/")):
  raise ValueError("第三方素材来源不可信："+url)
 if path.is_file() and (not size or path.stat().st_size==size) and (not md5 or digest(path,"md5")==md5):
  return
 path.parent.mkdir(parents=True,exist_ok=True)
 part=path.with_suffix(path.suffix+".part")
 for retry in range(8):
  try:
   req=Request(url,headers={"User-Agent":"Mozilla/5.0 (Windows NT 10.0; Win64; x64)","Accept":"*/*"})
   with urlopen(req,timeout=80) as stream,part.open("wb") as out:
    shutil.copyfileobj(stream,out,1024*1024)
   if size and part.stat().st_size!=size:raise IOError("下载字节数不符")
   if md5 and digest(part,"md5")!=md5:raise IOError("官方MD5不符")
   part.replace(path)
   return
  except Exception as exc:
   part.unlink(missing_ok=True)
   print(f"SCAN_RETRY {path.name} #{retry+1} {str(exc)[:75]}",flush=True)
   time.sleep(min(retry+1,8))
 raise RuntimeError("下载/官方签名未通过，停止制作："+str(path))

def scan_specs():
 all=[]
 for layer,slug in SOURCES.items():
  meta=OUT/("PolyHaven_"+slug+"_Files.json")
  if not meta.exists():fetch("https://api.polyhaven.com/files/"+slug,meta)
  official=read_json(meta)
  for role,upkey in ROLES.items():
   spec=official.get(upkey,{}).get("2k",{}).get("png")
   if not spec or not spec.get("md5") or not spec.get("url"):raise RuntimeError(layer+"/"+role+"缺少官方2k无损图")
   filename=f"T_DBA_Terrain_{layer}_{role}_Source.png"
   all.append({
    "layer":layer,"role":role,"slug":slug,"sourceUrl":spec["url"],
    "upstreamMd5":spec["md5"],"expectedBytes":int(spec.get("size",0)),
    "stagePath":(STAGE/layer/filename).relative_to(ROOT).as_posix(),
    "sourcePath":(PBR/layer/filename).relative_to(ROOT).as_posix(),
    "assetPath":f"/DBAWorldPack_Village/Terrain/Textures/{layer}/T_DBA_Terrain_{layer}_{role}",
    "resolution":[2048,2048],"compression":COMP[role],"sRGB":role=="BaseColor",
    "originUrl":"https://polyhaven.com/a/"+slug,"license":"CC0 1.0"})
 return all

def validate_one(spec):
 path=ROOT/spec["stagePath"]
 if not path.is_file() or (spec["expectedBytes"] and path.stat().st_size!=spec["expectedBytes"]):
  raise ValueError("源图文件大小不符："+str(path))
 if digest(path,"md5")!=spec["upstreamMd5"]:raise ValueError("官方签名不符："+str(path))
 with Image.open(path) as im:
  im.verify()
 with Image.open(path) as im:
  if im.format!="PNG" or im.size!=(2048,2048):raise ValueError("扫描贴图不是原生2K PNG："+str(path))
  if spec["role"]=="Height":
   if im.mode in ("L","I","I;16","I;16B","I;16L"):
    spec["heightPrecisionBits"]=16 if "16" in im.mode or im.mode=="I" else 8
   elif im.mode in ("RGB","RGBA"):
    # 部分官方高度PNG以相同RGB存灰度；拒绝真实多彩图或透明通道。
    r,g,b=im.getchannel("R"),im.getchannel("G"),im.getchannel("B")
    if ImageChops.difference(r,g).getbbox() or ImageChops.difference(r,b).getbbox():
     raise ValueError("高度RGB不是同值灰度："+str(path))
    if im.mode=="RGBA" and im.getchannel("A").getextrema()!=(255,255):
     raise ValueError("高度图RGBA存在透明区域："+str(path))
    spec["heightPrecisionBits"]=8
   else:raise ValueError("高度图不是独立灰度源")
  elif im.mode not in ("RGB","RGBA"):raise ValueError("PBR三通道图不是RGB源")

def stage(workers):
 OUT.mkdir(parents=True,exist_ok=True)
 specs=scan_specs()
 if len(specs)!=40:raise RuntimeError("十层四图不齐")
 print("TERRAIN_PBR_STAGE_BEGIN maps=40 source=PolyHaven_CC0",flush=True)
 def task(spec):
  path=ROOT/spec["stagePath"]
  fetch(spec["sourceUrl"],path,spec["expectedBytes"],spec["upstreamMd5"])
  validate_one(spec)
  spec["sha256"]=digest(path)
  return f'{spec["layer"]}/{spec["role"]}'
 with ThreadPoolExecutor(max_workers=workers) as pool:
  futures=[pool.submit(task,entry) for entry in specs]
  for future in as_completed(futures):print("TERRAIN_PBR_STAGE_MAP_PASS",future.result(),flush=True)
 write_json(PLAN,{"schemaVersion":1,"sourceKind":"official_polyhaven_CC0_matched_scan_maps","files":specs})
 print("TERRAIN_PBR_STAGE_40_PASS",flush=True)

def verify():
 if not PLAN.is_file():raise RuntimeError("尚未生成40张真实扫描暂存清单")
 plan=read_json(PLAN)
 specs=plan.get("files",[])
 if plan.get("sourceKind")!="official_polyhaven_CC0_matched_scan_maps" or len(specs)!=40:
  raise ValueError("扫描来源清单版本无效")
 by_layer={}
 for spec in specs:
  if SOURCES.get(spec["layer"])!=spec["slug"] or spec["role"] not in ROLES:raise ValueError("来源层混用")
  by_layer.setdefault(spec["layer"],set()).add(spec["role"])
  validate_one(spec)
  if digest(ROOT/spec["stagePath"])!=spec["sha256"]:raise ValueError("SHA256不符："+spec["stagePath"])
 if len(by_layer)!=10 or any(set(ROLES)!=roles for roles in by_layer.values()):
  raise ValueError("同源地表四图不完整")
 print("TERRAIN_PBR_STAGED_VERIFIED layers=10 maps=40",flush=True)
 return specs

def commit():
 specs=verify()
 old=read_json(OLD)
 if old.get("sourceKind")!="procedural_calibration_not_gaea" or len(old.get("files",[]))!=50:
  raise RuntimeError("源清单已改变：禁止对未知原图覆盖")
 records=[i for i in old["files"] if i["category"]=="PBR_TECHNICAL"]
 if len(records)!=30:raise ValueError("旧技术纹理不是30张")
 for entry in records:
  path=ROOT/entry["sourcePath"]
  if not path.is_file() or digest(path)!=entry["sha256"]:
   raise RuntimeError("旧校准PNG被其它会话更改："+str(path))
  backup=OUT/"Rollback_TerrainCalibration_SourceAndUEAssets.zip"
  expected=[OLD]+[ROOT/x["sourcePath"] for x in records]+[
      TEX/(x["assetPath"].split("/Terrain/Textures/",1)[1]+".uasset") for x in records]
  names={path.relative_to(ROOT).as_posix() for path in expected}
  if backup.exists():
   # 独立只读备份已提前生成：精确校验全部61项文件名、CRC和清单SHA。
   with zipfile.ZipFile(backup) as z:
    if set(z.namelist())!=names or z.testzip() is not None:
     raise RuntimeError("回退包完整性失败，拒绝继续")
    if hashlib.sha256(z.read(OLD.relative_to(ROOT).as_posix())).hexdigest()!=digest(OLD):
     raise RuntimeError("回退包历史清单不是当前工作区版本")
  else:
   temp=backup.with_suffix(".zip.tmp")
   with zipfile.ZipFile(temp,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=2) as z:
    for path in expected:
     if not path.is_file():raise FileNotFoundError("待备份的原始UE资产缺失："+str(path))
     z.write(path,path.relative_to(ROOT).as_posix())
   with zipfile.ZipFile(temp) as z:
    if set(z.namelist())!=names or z.testzip() is not None:raise RuntimeError("备份生成后校验失败")
   temp.replace(backup)
 # V1校准清单是Gaea V1/V2投影的来源SHA合同，禁止修改其字节。
 # 新Production清单作为PBR新真源，旧清单PBR记录仅供历史复现。
 for spec in specs:
  target=ROOT/spec["sourcePath"]
  target.parent.mkdir(parents=True,exist_ok=True)
  part=target.with_suffix(".png.new")
  shutil.copyfile(ROOT/spec["stagePath"],part)
  part.replace(target)
  if digest(target)!=spec["sha256"]:raise RuntimeError("落盘SHA异常："+str(target))
  print("TERRAIN_PBR_SOURCE_REPLACED",spec["layer"],spec["role"],flush=True)
 write_json(PROD,{"schemaVersion":1,"sourceKind":"official_polyhaven_CC0_matched_scan_maps",
  "license":"CC0 1.0","resolution":[2048,2048],
  "historicalCalibrationManifest":OLD.relative_to(ROOT).as_posix(),
  "historicalCalibrationManifestSHA256":digest(OLD),
  "historicalCalibrationPBRRowsSuperseded":True,
  "artApproval":"pending_UE_real_render_and_user_review",
  "rollbackArchive":backup.relative_to(ROOT).as_posix(),
  "files":specs})
 print("TERRAIN_PBR_SOURCE_40_REPLACED rollback="+str(backup),flush=True)

if __name__=="__main__":
 p=argparse.ArgumentParser(description="十种正式PBR扫描纹理的授权替换")
 mode=p.add_mutually_exclusive_group(required=True)
 mode.add_argument("--stage",action="store_true")
 mode.add_argument("--verify",action="store_true")
 mode.add_argument("--commit",action="store_true")
 p.add_argument("--workers",type=int,default=3)
 args=p.parse_args()
 if not 1<=args.workers<=4:p.error("下载并发须为1-4")
 if args.stage:stage(args.workers)
 if args.verify:verify()
 if args.commit:commit()
