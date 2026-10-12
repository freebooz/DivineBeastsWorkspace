#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8真实制作十层扫描PBR+可选高度融合+湿润/积雪/苔藓的客户端材质。

归属第三层DBAPresentationPack_Core（纯客户端表现内容），不能把含高精扫描纹理的
材质硬引用到正式Village共享地图或服务器。十层Landscape LayerInfo身份仍归
DBAWorldPack_Village；PCG使用相同LS_*权重，不直接生成视觉材质。
默认inspect；apply仅新增独立项目材质，不覆盖旧校准母材质；
verify由新编辑器进程对UClass、材质连线与实例父类回读。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCES=ROOT/"Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json"
MODE=os.getenv("TERRAIN_PRODUCTION_MATERIAL_MODE","inspect").lower().strip()
BASE="/DBAPresentationPack_Core/Materials/Terrain"
MASTER=BASE+"/M_DBA_Terrain_Production10"
INSTANCE=BASE+"/MI_DBA_Terrain_Production10_Review"
WORLD="/DBAWorldPack_Village"
LAYERS=("Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud")

def validate_manifest():
 if not SOURCES.exists():raise RuntimeError("缺少正式扫描PBR来源清单，不能制作正式材质")
 spec=json.loads(SOURCES.read_text(encoding="utf-8"))
 if spec.get("sourceKind")!="official_polyhaven_CC0_matched_scan_maps" or len(spec.get("files",[]))!=40:
  raise RuntimeError("扫描PBR来源合同不完整")
 got={(x["layer"],x["role"]):x["assetPath"] for x in spec["files"]}
 for layer in LAYERS:
  for role in ("BaseColor","Normal","ORM","Height"):
   if (layer,role) not in got:raise RuntimeError("缺少同源物理图："+layer+"/"+role)
 return got

def execute():
 if MODE not in ("inspect","apply","verify"):raise ValueError("模式限inspect/apply/verify")
 paths=validate_manifest()
 if MODE=="inspect":
  print("TERRAIN_PRODUCTION_MATERIAL_PLAN 10_layers_40_maps client_only_height_switch_moss_snow_wetness")
  return
 try:import unreal # type: ignore
 except ImportError as exc:raise RuntimeError("必须由UE5.8真实编辑器创建.uasset") from exc
 if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
  raise RuntimeError("非神兽联盟正式UE工程")
 lib=unreal.EditorAssetLibrary
 matlib=unreal.MaterialEditingLibrary
 assets=unreal.AssetToolsHelpers.get_asset_tools()
 texture={}
 for key,path in paths.items():
  obj=lib.load_asset(path)
  if not isinstance(obj,unreal.Texture2D):raise RuntimeError("未导入正式Texture2D："+path)
  texture[key]=obj
 def inspect():
  mat=lib.load_asset(MASTER)
  inst=lib.load_asset(INSTANCE)
  if not isinstance(mat,unreal.Material) or not isinstance(inst,unreal.MaterialInstanceConstant):
   raise RuntimeError("正式十层材质UMaterial或MI不存在")
  nodes=matlib.get_num_material_expressions(mat)
  if nodes<210:raise RuntimeError("已保存正式PBR母材质节点数不符："+str(nodes))
  if inst.get_editor_property("parent")!=mat:raise RuntimeError("项目MI父材质错误")
  print("TERRAIN_PRODUCTION_MATERIAL_UE_VERIFIED",MASTER,"nodes",nodes)
  return
 if lib.does_asset_exist(MASTER) or lib.does_asset_exist(INSTANCE):
  inspect()
  return
 if MODE=="verify":raise FileNotFoundError("独立正式材质尚未生成")
 if not lib.does_directory_exist(BASE):lib.make_directory(BASE)
 mat=assets.create_asset(MASTER.rsplit("/",1)[1],BASE,unreal.Material,unreal.MaterialFactoryNew())
 if not isinstance(mat,unreal.Material):raise RuntimeError("UE未创建真实UMaterial")
 def node(cls,x,y):
  e=matlib.create_material_expression(mat,cls,int(x),int(y))
  if e is None:raise RuntimeError("UMaterial节点创建失败："+str(cls))
  return e
 def wire(src,dst,pin,out=""):
  ok=matlib.connect_material_expressions(src,out,dst,pin)
  if not ok:raise RuntimeError("材质图节点未连线："+src.get_name()+"→"+pin)
 def value(v,x,y):
  e=node(unreal.MaterialExpressionConstant,x,y)
  e.set_editor_property("r",float(v))
  return e
 def vec(r,g,b,x,y):
  e=node(unreal.MaterialExpressionConstant3Vector,x,y)
  e.set_editor_property("constant",unreal.LinearColor(r,g,b,1))
  return e
 def scalar(name,v,x,y):
  e=node(unreal.MaterialExpressionScalarParameter,x,y)
  e.set_editor_property("parameter_name",unreal.Name(name))
  e.set_editor_property("default_value",float(v))
  return e
 def op(cls,a,b,x,y,outa="",outb=""):
  e=node(cls,x,y)
  wire(a,e,"A",outa)
  wire(b,e,"B",outb)
  return e
 def sat(a,x,y):
  e=node(unreal.MaterialExpressionSaturate,x,y)
  wire(a,e,"Input")
  return e
 def lerp(a,b,w,x,y,outa="",outb="",outw=""):
  e=node(unreal.MaterialExpressionLinearInterpolate,x,y)
  wire(a,e,"A",outa)
  wire(b,e,"B",outb)
  wire(w,e,"Alpha",outw)
  return e
 def sample(tex,name,uv,x,y):
  e=node(unreal.MaterialExpressionTextureSampleParameter2D,x,y)
  e.set_editor_property("parameter_name",unreal.Name(name))
  e.set_editor_property("texture",tex)
  wire(uv,e,"Coordinates")
  return e
 def add(a,b,x,y,oa="",ob=""):return op(unreal.MaterialExpressionAdd,a,b,x,y,oa,ob)
 def mul(a,b,x,y,oa="",ob=""):return op(unreal.MaterialExpressionMultiply,a,b,x,y,oa,ob)
 def sub(a,b,x,y,oa="",ob=""):return op(unreal.MaterialExpressionSubtract,a,b,x,y,oa,ob)
 def norm(a,x,y):
  e=node(unreal.MaterialExpressionNormalize,x,y)
  wire(a,e,"VectorInput")
  return e
 def fold(elements,x,y):
  out=elements[0]
  for i,v in enumerate(elements[1:],1):out=add(out,v,x+170*i,y+95*i)
  return out
 # 独立十层LayerSample，来源与Gaea/PCG校准权重严格同名。
 uv=node(unreal.MaterialExpressionTextureCoordinate,-2400,-2300)
 tile=scalar("TerrainDetailTiling",12,-2200,-2180)
 txuv=mul(uv,tile,-2010,-2200)
 zero=value(0,430,-2700)
 one=value(1,430,-2560)
 half=value(.5,450,-2390)
 height_strength=scalar("TerrainHeightBlendStrength",.25,-2050,-1900)
 weights=[];colors=[];normals=[];roughs=[];occlusions=[]
 masks_by_layer={}
 for i,layer in enumerate(LAYERS):
  y=i*800-2400
  weight=node(unreal.MaterialExpressionLandscapeLayerSample,-1740,y)
  weight.set_editor_property("parameter_name",unreal.Name("LS_"+layer))
  weight.set_editor_property("preview_weight",1.0 if layer=="Dirt" else 0.0)
  color=sample(texture[(layer,"BaseColor")],"Terrain"+layer+"BaseColor",txuv,-1440,y-160)
  normal=sample(texture[(layer,"Normal")],"Terrain"+layer+"Normal",txuv,-1440,y+70)
  orm=sample(texture[(layer,"ORM")],"Terrain"+layer+"ORM",txuv,-1440,y+260)
  height=sample(texture[(layer,"Height")],"Terrain"+layer+"Height",txuv,-1440,y+470)
  # 高度源仅用于邻层微表面过渡；开关关闭时编译器剥离Height采样。
  hb=sub(height,half,-1060,y+475,"R")
  hb=mul(hb,height_strength,-900,y+470)
  hb=add(hb,one,-740,y+470)
  elevated=mul(weight,hb,-540,y+460)
  toggle=node(unreal.MaterialExpressionStaticSwitchParameter,-350,y+475)
  toggle.set_editor_property("parameter_name",unreal.Name("TerrainEnableHeightBlend"))
  toggle.set_editor_property("default_value",False)
  wire(elevated,toggle,"A")
  wire(weight,toggle,"B")
  weights.append(toggle)
  masks_by_layer[layer]=toggle
  colors.append(mul(color,toggle,-350,y-165,"RGB"))
  normals.append(mul(normal,toggle,-330,y+50,"RGB"))
  roughs.append(mul(orm,toggle,-330,y+255,"G"))
  occlusions.append(mul(orm,toggle,-310,y+355,"R"))
 sum_w=fold(weights,250,-2100)
 # 不能简单把十组法线RGB混合后直接输出，必须经过Normalize保证单位长度。
 sum_c=fold(colors,250,-1150)
 sum_n=fold(normals,250,30)
 sum_r=fold(roughs,250,1200)
 sum_ao=fold(occlusions,250,2600)
 missing=sat(sub(one,sum_w,1830,-2200),2050,-2200)
 dirt=sample(texture[("Dirt","BaseColor")],"TerrainFallbackDirt",txuv,1850,-1250)
 col=add(sum_c,mul(dirt,missing,2150,-1250,"RGB"),2550,-1080)
 flat=vec(0,0,1,1830,130)
 n=norm(add(sum_n,mul(flat,missing,2060,250),2560,250),2780,500)
 rough=add(sum_r,mul(value(.82,1780,1200),missing,2140,1350),2600,1300)
 ao=add(sum_ao,missing,2600,2700)
  # 本材质仅属于项目ClientOnly表现包，可引用平台Surface真实雪苔PBR纹理。
  # 共享Village地图仍使用中性基础材质，不能硬引用本高精纯表现母材质。
  def platform_texture(relative):
   path="/GamePlatformSurface/Textures/"+relative
   obj=lib.load_asset(path)
   if not isinstance(obj,unreal.Texture2D):
    raise RuntimeError("平台通用PBR缺失："+path)
   return obj
  moss_texture={
   "BaseColor":platform_texture("Moss/T_GP_Moss_Albedo"),
   "Normal":platform_texture("Moss/T_GP_Moss_Normal"),
   "ORM":platform_texture("Moss/T_GP_Moss_Orm"),
   "Coverage":platform_texture("Moss/T_GP_Moss_Coverage")}
  moss=scalar("TerrainMossAmount",0,2810,-2250)
  moss_weight=sat(add(masks_by_layer["Rock"],masks_by_layer["ForestSoil"],2820,-2070),3010,-2070)
  moss_uv=mul(uv,scalar("TerrainMossMacroTiling",2,3090,-2640),3240,-2670)
  moss_noise=sample(moss_texture["Coverage"],"TerrainMossCoverage",moss_uv,3180,-2400)
  moss_strength=mul(moss,moss_weight,3250,-2040)
  moss_mask=sat(mul(moss_strength,moss_noise,3470,-2070,ob="R"),3650,-2070)
  moss_c=sample(moss_texture["BaseColor"],"TerrainMossAlbedo",txuv,2830,-1410)
  moss_n=sample(moss_texture["Normal"],"TerrainMossNormal",txuv,2830,370)
  moss_orm=sample(moss_texture["ORM"],"TerrainMossORM",txuv,2830,1720)
  col=lerp(col,moss_c,moss_mask,3700,-1050,outb="RGB")
  n=norm(lerp(n,moss_n,moss_mask,3700,550,outb="RGB"),3900,750)
  rough=lerp(rough,moss_orm,moss_mask,3750,1500,outb="G")
  ao=lerp(ao,moss_orm,moss_mask,3750,2680,outb="R")
 # 局部地表湿润为视觉光学变化：略暗、降低Roughness，不决定玩法权威碰撞。
 wet=scalar("VillageWetness",0,3300,3200)
 shade=sub(one,mul(wet,value(.23,3460,3200),3700,3140),3960,3050)
 col=mul(col,shade,4180,-900)
 rough=lerp(rough,value(.36,4100,1620),wet,4390,1550)
 # 保留VillageSnow的参数名，便于客户端世界MID用同一权威天气快照。
  snow_cov=platform_texture("Snow/T_GP_Snow_CoverageNoise")
  snow_col=platform_texture("Weather/T_GP_Weather_SnowAlbedo")
  snow_nrm=platform_texture("Weather/T_GP_Weather_SnowNormal")
  # 独立纯客户端审核场景默认取平台真实雪纹理；运行时可覆盖同名参数。
 snow_amount=scalar("VillageSnowAmount",0,3610,-2580)
 vn=node(unreal.MaterialExpressionVertexNormalWS,3590,-2950)
 dz=node(unreal.MaterialExpressionDotProduct,3830,-2850)
 wire(vn,dz,"A");wire(vec(0,0,1,3610,-2750),dz,"B")
 snow_slope=sat(dz,4040,-2850)
 snow_uv=mul(uv,scalar("TerrainSnowMacroTiling",2.2,3960,-2520),4210,-2530)
 snow_noise=sample(snow_cov,"VillageSnowCoverageTexture",snow_uv,4380,-2670)
 coverage=sat(mul(mul(snow_amount,snow_slope,4610,-2500),snow_noise,4790,-2510,ob="R"),4950,-2500)
 snow_color=sample(snow_col,"VillageSnowAlbedoTexture",txuv,4530,-1300)
 snow_norm=sample(snow_nrm,"VillageSnowNormalTexture",txuv,4530,330)
 col=lerp(col,snow_color,coverage,5150,-760,outb="RGB")
 n=norm(lerp(n,snow_norm,coverage,5150,480,outb="RGB"),5340,730)
 rough=lerp(rough,value(.88,4700,1850),coverage,5150,1700)
 ao=lerp(ao,one,coverage,5180,2740)
 out=[(col,unreal.MaterialProperty.MP_BASE_COLOR),
      (n,unreal.MaterialProperty.MP_NORMAL),
      (rough,unreal.MaterialProperty.MP_ROUGHNESS),
      (ao,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)]
 for expression,prop in out:
  if not matlib.connect_material_property(expression,"",prop):
   raise RuntimeError("物理材质最终输出未连接："+str(prop))
 diagnostics=[str(x) for x in matlib.recompile_material(mat)]
 if any("error" in x.lower() for x in diagnostics):
  raise RuntimeError("UE5.8 Shader编译有错误："+str(diagnostics))
 if not lib.save_loaded_asset(mat,only_if_is_dirty=False):
  raise RuntimeError("Material保存失败")
 inst=assets.create_asset(INSTANCE.rsplit("/",1)[1],BASE,
                          unreal.MaterialInstanceConstant,
                          unreal.MaterialInstanceConstantFactoryNew())
 if not isinstance(inst,unreal.MaterialInstanceConstant):
  raise RuntimeError("Material Instance创建失败")
 matlib.set_material_instance_parent(inst,mat)
 matlib.set_material_instance_scalar_parameter_value(inst,"VillageSnowAmount",0)
 matlib.set_material_instance_scalar_parameter_value(inst,"TerrainMossAmount",0)
 if not lib.save_loaded_asset(inst,only_if_is_dirty=False):
  raise RuntimeError("Material Instance保存失败")
 inspect()

if __name__=="__main__":execute()
