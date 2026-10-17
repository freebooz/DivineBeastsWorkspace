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
# 明确的可选候选版本：原正式高精母材质与审核MI均不覆盖；保持十层LayerInfo和所有纹理参数名。
FUNCTION_INTEGRATION=os.getenv("TERRAIN_FUNCTION_INTEGRATION","0").strip()=="1"
BASE="/DBAPresentationPack_Core/Materials/Terrain"
MASTER=BASE+("/M_DBA_Terrain_Production10_FunctionIntegrated" if FUNCTION_INTEGRATION
             else "/M_DBA_Terrain_Production10")
INSTANCE=BASE+("/MI_DBA_Terrain_Production10_FunctionIntegrated_Review" if FUNCTION_INTEGRATION
               else "/MI_DBA_Terrain_Production10_Review")
WORLD="/DBAWorldPack_Village"

# 跨项目平台材质函数稳定合同：调用前核对真实输入/输出引脚，禁止函数资产改签后静默错连。
SHARED_FUNCTIONS={
 "MacroColor":("Landscape/MF_GP_LandscapeMacroColor",
               {"BaseColor","MacroTint","MacroNoise","Strength"},{"Color"}),
 "DistanceDetail":("Landscape/MF_GP_LandscapeDistanceDetail",
                   {"NearColor","FarColor","StartDistanceCm","EndDistanceCm"},
                   {"DetailColor","FarWeight"}),
 "SlopeLayers":("Landscape/MF_GP_LandscapeSlopeLayers",
                {"SlopeNormalized","SlopeStart","SlopeEnd"},{"Mask"}),
 "AltitudeLayers":("Landscape/MF_GP_LandscapeAltitudeLayers",
                   {"HeightCm","LowerHeightCm","UpperHeightCm"},{"Mask"}),
 "BiomeMask":("Landscape/MF_GP_LandscapeBiomeMask",
              {"BiomeWeight","SlopeMask","AltitudeMask","LocalWeight"},{"BiomeMask"}),
 "PBRLayer":("Layers/MF_GP_PBRLayerBlend",
             {"BaseColor","OverlayColor","BaseNormal","OverlayNormal",
              "BaseRoughness","OverlayRoughness","BaseAO","OverlayAO","Coverage"},
             {"BaseColor","Normal","Roughness","AO"}),
}
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
  print("TERRAIN_PRODUCTION_MATERIAL_PLAN 10_layers_40_maps client_only_height_switch_moss_snow_wetness",
        "function_integrated_candidate="+str(FUNCTION_INTEGRATION),MASTER)
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
 shared_functions={}
 if FUNCTION_INTEGRATION:
  for key,(relative,expected_inputs,expected_outputs) in SHARED_FUNCTIONS.items():
   path="/GamePlatformSurface/MaterialFunctions/"+relative
   function=lib.load_asset(path)
   if not isinstance(function,unreal.MaterialFunction):
    raise RuntimeError("缺少可复用平台MaterialFunction："+path)
   nodes=matlib.get_material_function_expressions(function)
   input_names={str(n.get_editor_property("input_name")) for n in nodes
                if isinstance(n,unreal.MaterialExpressionFunctionInput)}
   output_names={str(n.get_editor_property("output_name")) for n in nodes
                 if isinstance(n,unreal.MaterialExpressionFunctionOutput)}
   if input_names!=expected_inputs or output_names!=expected_outputs:
    raise RuntimeError("平台材质函数引脚合同发生变化，拒绝接入："+path+" "+
                       str((input_names,output_names)))
   shared_functions[key]=function
 def inspect():
  mat=lib.load_asset(MASTER)
  inst=lib.load_asset(INSTANCE)
  if not isinstance(mat,unreal.Material) or not isinstance(inst,unreal.MaterialInstanceConstant):
   raise RuntimeError("正式十层材质UMaterial或MI不存在")
  nodes=matlib.get_num_material_expressions(mat)
  if nodes<210:raise RuntimeError("已保存正式PBR母材质节点数不符："+str(nodes))
  if inst.get_editor_property("parent")!=mat:raise RuntimeError("项目MI父材质错误")
  if FUNCTION_INTEGRATION:
   calls=[e for e in matlib.get_material_expressions(mat)
          if isinstance(e,unreal.MaterialExpressionMaterialFunctionCall)]
   actual={}
   for expr in calls:
    function=expr.get_editor_property("material_function")
    if not isinstance(function,unreal.MaterialFunction):
     raise RuntimeError("材质存在尚未绑定函数的MaterialFunctionCall节点")
    path=str(function.get_path_name()).split(".")[0]
    actual[path]=actual.get(path,0)+1
   expected={"/GamePlatformSurface/MaterialFunctions/"+v[0]:(3 if k=="PBRLayer" else 1)
             for k,v in SHARED_FUNCTIONS.items()}
   if actual!=expected:
    raise RuntimeError("函数化十层母材质缺少真实调用或次数不符："+str((actual,expected)))
  print("TERRAIN_PRODUCTION_MATERIAL_UE_VERIFIED",MASTER,"nodes",nodes,
        "function_calls="+str(len(calls)) if FUNCTION_INTEGRATION else "legacy_graph")
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
  # UE5.8中TextureSampleParameter2D的编辑器暴露名有时是UVs，而原生GetInputName为Coordinates。
  # 延续当前已成功编译的十层校准作者工具兼容方式，限定仅坐标/通用一号输入回退。
  if not ok and pin=="Coordinates":
   ok=matlib.connect_material_expressions(src,out,dst,"UVs")
  if not ok and pin in ("Coordinates","Input","VectorInput"):
   ok=matlib.connect_material_expressions(src,out,dst,"")
  if not ok:raise RuntimeError("材质图节点未连线："+src.get_name()+"→"+pin)
 def call_shared(name,inputs,x,y):
  """为候选材质创建真实MaterialFunctionCall并强制完成所有预期输入。"""
  if not FUNCTION_INTEGRATION or name not in shared_functions:
   raise RuntimeError("平台材质函数未经过完整接口预检："+name)
  expected=SHARED_FUNCTIONS[name][1]
  if set(inputs)!=expected:
   raise RuntimeError("平台材质函数输入不完整："+name+" "+str(set(inputs)^expected))
  call=node(unreal.MaterialExpressionMaterialFunctionCall,x,y)
  if not call.set_material_function(shared_functions[name]):
   raise RuntimeError("平台函数绑定失败："+name)
  for pin,(source,output) in inputs.items():
   wire(source,call,pin,output)
  return call
 def extract_shared(source,output,x,y):
  """使用透传节点把多输出函数结果变为普通表达式，后续层共享同一张图。"""
  reroute=node(unreal.MaterialExpressionReroute,x,y)
  wire(source,reroute,"Input",output)
  return reroute
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
  compression=tex.get_editor_property("compression_settings")
  is_srgb=bool(tex.get_editor_property("srgb"))
  # UE5.8 MaterialExpressionUtils::GetSamplerTypeForTexture官方规则：
  # 非sRGB的TC_Grayscale为LinearGrayscale；缺少Linear将导致Shader失败。
  sampler_types={
   unreal.TextureCompressionSettings.TC_DEFAULT:(
    unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if is_srgb else
    unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR),
   unreal.TextureCompressionSettings.TC_NORMALMAP:unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
   unreal.TextureCompressionSettings.TC_MASKS:unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
   unreal.TextureCompressionSettings.TC_GRAYSCALE:(
    unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if is_srgb else
    unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
  }
  if compression not in sampler_types:
   raise RuntimeError("不支持的PBR采样压缩类型："+name+" "+str(compression))
  e.set_editor_property("sampler_type",sampler_types[compression])
  # UE5.8 Shared Wrap共用采样器，避免10层纹理超过硬件独立槽预算。
  shared=getattr(unreal.SamplerSourceMode,"SSM_WRAP_WORLD_GROUP_SETTINGS",None)
  if shared is None:
   raise RuntimeError("UE未提供共享Wrap采样器，禁止创建超预算材质")
  e.set_editor_property("sampler_source",shared)
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
 def pbr_layer(color,normal,roughness,ao,overlay_color,overlay_normal,
               overlay_roughness,overlay_ao,mask,x,y):
  """共享平台四通道覆盖函数，保留各源PBR贴图，而不是套用旧函数固定色值。"""
  call=call_shared("PBRLayer",{
   "BaseColor":(color,""),"OverlayColor":overlay_color,
   "BaseNormal":(normal,""),"OverlayNormal":overlay_normal,
   "BaseRoughness":(roughness,""),"OverlayRoughness":overlay_roughness,
   "BaseAO":(ao,""),"OverlayAO":overlay_ao,"Coverage":(mask,"")},x,y)
  return (extract_shared(call,"BaseColor",x+245,y-220),
          extract_shared(call,"Normal",x+245,y-60),
          extract_shared(call,"Roughness",x+245,y+100),
          extract_shared(call,"AO",x+245,y+260))
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
  # UE5.8 GetInputName原生返回True/False，不能使用内部成员名A/B连线。
  wire(elevated,toggle,"True")
  wire(weight,toggle,"False")
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
 # 可选HeightBlend会改变权重和，应避免多层过曝/法线长度错误。
 # 权重合计通常为1；高度增强导致>1时统一除以总和，小于1时仍由Dirt兜底。
 sum_norm=node(unreal.MaterialExpressionMax,1700,-2540)
 wire(sum_w,sum_norm,"A")
 wire(one,sum_norm,"B")
 def normalize_weighted(expr,x,y):
  return op(unreal.MaterialExpressionDivide,expr,sum_norm,x,y)
 sum_c=normalize_weighted(sum_c,1920,-1130)
 sum_n=normalize_weighted(sum_n,1930,10)
 sum_r=normalize_weighted(sum_r,1930,1240)
 sum_ao=normalize_weighted(sum_ao,1950,2650)
 missing=sat(sub(one,sum_w,1830,-2200),2050,-2200)
 dirt=sample(texture[("Dirt","BaseColor")],"TerrainFallbackDirt",txuv,1850,-1250)
 col=add(sum_c,mul(dirt,missing,2150,-1250,"RGB"),2550,-1080)
 flat=vec(0,0,1,1830,130)
 n=norm(add(sum_n,mul(flat,missing,2060,250),2560,250),2780,500)
 rough=add(sum_r,mul(value(.82,1780,1200),missing,2140,1350),2600,1300)
 ao=sat(add(sum_ao,missing,2600,2700),2760,2790)
 # 本母材质归纯客户端表现包，可直接使用平台GamePlatformSurface真实PBR贴图。
 # 正式Village共享地图继续使用中性基础材质，绝不硬引用本高精母材质。
 def platform_texture(relative):
  path="/GamePlatformSurface/Textures/"+relative
  obj=lib.load_asset(path)
  if not isinstance(obj,unreal.Texture2D):
   raise RuntimeError("平台通用PBR纹理缺失："+path)
  return obj
 # 直接消费已有平台MPC（材质参数集合），不建立第二套天气权威。
 collection=lib.load_asset("/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal")
 if not isinstance(collection,unreal.MaterialParameterCollection):
  raise RuntimeError("GamePlatformSurface全局参数集合未加载，禁止创建重复状态机制")
 def global_scalar(name,x,y):
  expr=node(unreal.MaterialExpressionCollectionParameter,x,y)
  expr.set_editor_property("collection",collection)
  expr.set_editor_property("parameter_name",unreal.Name(name))
  return expr
 def maximum(a,b,x,y):
  return op(unreal.MaterialExpressionMax,a,b,x,y)
 moss_texture={
  "BaseColor":platform_texture("Moss/T_GP_Moss_Albedo"),
  "Normal":platform_texture("Moss/T_GP_Moss_Normal"),
  "ORM":platform_texture("Moss/T_GP_Moss_Orm"),
  "Coverage":platform_texture("Moss/T_GP_Moss_Coverage")}
 moss=mul(scalar("TerrainMossAmount",0,2810,-2250),
          global_scalar("GP_Surface_GlobalMossInfluence",2610,-2570),2790,-2510)
 moss_weight=sat(add(masks_by_layer["Rock"],masks_by_layer["ForestSoil"],2820,-2070),3010,-2070)
 moss_uv=mul(uv,scalar("TerrainMossMacroTiling",2,3090,-2640),3240,-2670)
 moss_noise=sample(moss_texture["Coverage"],"TerrainMossCoverage",moss_uv,3180,-2400)
 if FUNCTION_INTEGRATION:
  # 消费平台宏观颜色函数、距离细节函数；默认轻微变化避免覆盖真实扫描PBR色相。
  macro_tint=mul(col,vec(.94,.97,.95,2940,-960),3050,-840)
  macro_call=call_shared("MacroColor",{
   "BaseColor":(col,""),"MacroTint":(macro_tint,""),
   "MacroNoise":(moss_noise,"R"),
   "Strength":(scalar("TerrainMacroColorStrength",.18,3100,-730),"")},
   3400,-850)
  macro_color=extract_shared(macro_call,"Color",3610,-850)
  far_color=mul(macro_color,vec(.97,.99,1.0,3510,-590),3740,-600)
  distance_call=call_shared("DistanceDetail",{
   "NearColor":(macro_color,""),"FarColor":(far_color,""),
   "StartDistanceCm":(scalar("TerrainDistanceFadeStartCm",4000,3530,-350),""),
   "EndDistanceCm":(scalar("TerrainDistanceFadeEndCm",18000,3560,-130),"")},
   4020,-670)
  col=extract_shared(distance_call,"DetailColor",4200,-710)
  biome_call=call_shared("BiomeMask",{
   "BiomeWeight":(moss_weight,""),
   "SlopeMask":(one,""),"AltitudeMask":(one,""),
   "LocalWeight":(moss_noise,"R")},3450,-2000)
  moss_mask=sat(mul(moss,biome_call,3680,-1980,ob="BiomeMask"),3890,-1960)
 else:
  moss_strength=mul(moss,moss_weight,3250,-2040)
  moss_mask=sat(mul(moss_strength,moss_noise,3470,-2070,ob="R"),3650,-2070)
 moss_c=sample(moss_texture["BaseColor"],"TerrainMossAlbedo",txuv,2830,-1410)
 moss_n=sample(moss_texture["Normal"],"TerrainMossNormal",txuv,2830,370)
 moss_orm=sample(moss_texture["ORM"],"TerrainMossORM",txuv,2830,1720)
 if FUNCTION_INTEGRATION:
  col,n,rough,ao=pbr_layer(col,n,rough,ao,(moss_c,"RGB"),(moss_n,"RGB"),
                            (moss_orm,"G"),(moss_orm,"R"),moss_mask,4280,-1700)
 else:
  col=lerp(col,moss_c,moss_mask,3700,-1050,outb="RGB")
  n=norm(lerp(n,moss_n,moss_mask,3700,550,outb="RGB"),3900,750)
  rough=lerp(rough,moss_orm,moss_mask,3750,1500,outb="G")
  ao=lerp(ao,moss_orm,moss_mask,3750,2680,outb="R")
 # 局部湿润只调整颜色和粗糙度，绝不改变服务器权威碰撞或玩法事实。
 wet=sat(maximum(scalar("VillageWetness",0,3300,3200),
                 global_scalar("GP_Surface_GlobalWetness",3000,3350),3500,3400),3700,3360)
 shade=sub(one,mul(wet,value(.23,3460,3200),3700,3140),3960,3050)
 if FUNCTION_INTEGRATION:
  wet_color=mul(col,shade,4530,-860)
  # 湿润对微法线仅做轻度平滑，遮罩由PBRLayer统一应用一次。
  wet_normal=norm(lerp(n,vec(0,0,1,4380,520),
                        value(.22,4330,640),4500,580),4670,700)
  col,n,rough,ao=pbr_layer(col,n,rough,ao,(wet_color,""),(wet_normal,""),
                            (value(.36,4400,1500),""),(ao,""),wet,4800,950)
 else:
  col=mul(col,shade,4180,-900)
  rough=lerp(rough,value(.36,4100,1620),wet,4390,1550)
 # 与既有VillageSnow世界桥接字段保持一致，默认使用平台正式雪纹理。
 snow_cov=platform_texture("Snow/T_GP_Snow_CoverageNoise")
 snow_col=platform_texture("Weather/T_GP_Weather_SnowAlbedo")
 snow_nrm=platform_texture("Weather/T_GP_Weather_SnowNormal")
 # 三张纹理可由客户端MID按软资源租约覆盖；审核地图只读默认纹理可见性。
 snow_amount=sat(maximum(scalar("VillageSnowAmount",0,3610,-2580),
                     global_scalar("GP_Surface_GlobalSnowAmount",3370,-3130),3830,-3160),4030,-3190)
 vn=node(unreal.MaterialExpressionVertexNormalWS,3590,-2950)
 dz=node(unreal.MaterialExpressionDotProduct,3830,-2850)
 wire(vn,dz,"A");wire(vec(0,0,1,3610,-2750),dz,"B")
 if FUNCTION_INTEGRATION:
  # 平台坡度/海拔函数由VertexNormalWS、绝对世界Z驱动，避免PixelNormalWS反向依赖Normal。
  slope_call=call_shared("SlopeLayers",{
   "SlopeNormalized":(dz,""),
   "SlopeStart":(scalar("TerrainSnowSlopeStart",.18,3860,-2830),""),
   "SlopeEnd":(scalar("TerrainSnowSlopeEnd",.88,3850,-2630),"")},
   4180,-2890)
  snow_slope=extract_shared(slope_call,"Mask",4370,-2890)
  world_position=node(unreal.MaterialExpressionWorldPosition,3870,-3410)
  world_height=node(unreal.MaterialExpressionComponentMask,4070,-3390)
  world_height.set_editor_property("b",True)
  wire(world_position,world_height,"Input")
  altitude_call=call_shared("AltitudeLayers",{
   "HeightCm":(world_height,""),
   "LowerHeightCm":(scalar("TerrainSnowHeightStartCm",-200000,3930,-3300),""),
   "UpperHeightCm":(scalar("TerrainSnowHeightEndCm",-199000,3930,-3100),"")},
   4370,-3300)
  snow_altitude=extract_shared(altitude_call,"Mask",4560,-3320)
 else:
  snow_slope=sat(dz,4040,-2850)
 snow_uv=mul(uv,scalar("TerrainSnowMacroTiling",2.2,3960,-2520),4210,-2530)
 snow_noise=sample(snow_cov,"VillageSnowCoverageTexture",snow_uv,4380,-2670)
 snow_coverage=mul(mul(snow_amount,snow_slope,4610,-2500),snow_noise,4790,-2510,ob="R")
 if FUNCTION_INTEGRATION:
  snow_coverage=mul(snow_coverage,snow_altitude,4930,-2720)
 coverage=sat(snow_coverage,4950,-2500)
 snow_color=sample(snow_col,"VillageSnowAlbedoTexture",txuv,4530,-1300)
 snow_norm=sample(snow_nrm,"VillageSnowNormalTexture",txuv,4530,330)
 if FUNCTION_INTEGRATION:
  col,n,rough,ao=pbr_layer(col,n,rough,ao,(snow_color,"RGB"),(snow_norm,"RGB"),
                            (value(.88,4700,1850),""),(one,""),coverage,5530,100)
 else:
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
 matlib.set_material_instance_scalar_parameter_value(inst,"TerrainMossAmount",0.48)
 if not lib.save_loaded_asset(inst,only_if_is_dirty=False):
  raise RuntimeError("Material Instance保存失败")
 inspect()

if __name__=="__main__":execute()
