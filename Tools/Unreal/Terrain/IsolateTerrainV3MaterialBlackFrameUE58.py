#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3地形全黑最小原生材质隔离实验（D3D12 UE5.8）。

针对独立审核Landscape，先采集原生V3材质，再只在瞬态Editor World临时
替换为引擎自带WorldGridMaterial（网格材质），捕获第二帧。
随后在瞬态世界插入一个使用引擎DefaultMaterial（默认材质）的100m可见Cube，
作为第三个硬编码几何/视口/RHI正向控制。只在内存替换/恢复LandscapeMaterial，
不保存地图、包、PCG、资源、原有图像或用户编辑器。
全部输出须依照真实像素检查，不冒充GPU完成或人工美术签字。
"""
from __future__ import annotations
import math
import json
from pathlib import Path

MAP="/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/"Game/Saved/Validation/TerrainV3MaterialIsolationCameraFixed_20261018"

def run():
 import unreal # type: ignore
 cmd=unreal.SystemLibrary.get_command_line().lower()
 if "-nullrhi" in cmd or "-d3d11" in cmd:
  raise RuntimeError("必须是真D3D12 RHI，禁止空设备")
 editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 if not editor or not editor.load_level(MAP):
  raise RuntimeError("UE5.8独立QA地图不可加载")
 world=unreal.EditorLevelLibrary.get_editor_world()
 a=unreal.EditorLevelLibrary.get_all_level_actors()
 landscapes=[x for x in a if isinstance(x,unreal.LandscapeProxy)]
 if len(landscapes)!=1:
  raise RuntimeError("QA Landscape数量必须为1")
 target=landscapes[0]
 old=target.get_editor_property("landscape_material")
 if "MI_DBA_Terrain_Production10_V3_Review" not in old.get_path_name():
  raise RuntimeError("不是原指定V3审查MI，拒绝临时覆盖")
 lib=unreal.EditorAssetLibrary
 known=lib.load_asset("/Engine/EngineMaterials/WorldGridMaterial")
 default=lib.load_asset("/Engine/EngineMaterials/DefaultMaterial")
 cube=lib.load_asset("/Engine/BasicShapes/Cube")
 if not isinstance(known,unreal.Material) or not isinstance(default,unreal.Material):
  raise RuntimeError("引擎WorldGrid/DefaultMaterial未发现")
 if not isinstance(cube,unreal.StaticMesh):
  raise RuntimeError("引擎基准Cube网格未找到")
 center,extent=target.get_actor_bounds(only_colliding_components=False)
 d=max(extent.x,extent.y)
 point=unreal.Vector(center.x+.30*d,center.y-.50*d,
                     center.z+.72*d+max(extent.z,6000))
 displacement=unreal.Vector(center.x-point.x,center.y-point.y,center.z-point.z)
 # 不能用Rotator(Pitch,Yaw,Roll)位置参数（UE Python是Roll/Pitch/Yaw）。
 angle=unreal.MathLibrary.find_look_at_rotation(point,center)
 forward=unreal.MathLibrary.get_forward_vector(angle)
 unit=math.sqrt(displacement.x**2+displacement.y**2+displacement.z**2)
 alignment=(forward.x*displacement.x+forward.y*displacement.y+forward.z*displacement.z)/unit
 print("SSLM_V3_MATERIAL_ISOLATION_CAMERA_ALIGNMENT",round(alignment,6),flush=True)
 if alignment<.999:
  raise RuntimeError("UE摄像机实际上没有对准QA Landscape")
 cap=None
 block=None
 OUT.mkdir(parents=True,exist_ok=True)
 photos=[]
 for name in ("V3Material","GridMaterial","GridPlusCube"):
  if (OUT/(name+"_D3D12_1280x720.png")).exists():
   raise FileExistsError("实验图已存在，拒绝覆盖："+name)
 try:
  cap=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D,point,angle)
  if not isinstance(cap,unreal.SceneCapture2D):
   raise RuntimeError("创建UE原生SceneCapture2D失败")
  comp=cap.get_editor_property("capture_component2d")
  rt=unreal.RenderingLibrary.create_render_target2d(
   world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8,
   unreal.LinearColor(0,0,0,1),False,False)
  comp.set_editor_property("texture_target",rt)
  comp.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_BASE_COLOR)
  comp.set_editor_property("fov_angle",65.)
  comp.set_editor_property("capture_every_frame",False)
  comp.set_editor_property("capture_on_movement",False)
  def capture(name):
   comp.capture_scene()
   file=OUT/(name+"_D3D12_1280x720.png")
   unreal.RenderingLibrary.export_render_target(world,rt,str(OUT),file.name)
   if not file.is_file():
    raise RuntimeError("引擎没有产生有效RenderTarget导出文件："+name)
   print("SSLM_V3_MATERIAL_ISOLATION_FRAME",name,"bytes",file.stat().st_size,flush=True)
   photos.append({"name":name,"file":str(file),"bytes":file.stat().st_size})
  capture("V3Material")
  target.set_editor_property("landscape_material",known)
  after=target.get_editor_property("landscape_material")
  if after!=known:
   raise RuntimeError("QA瞬态母材质未被UE编辑器成功切换")
  capture("GridMaterial")
  block=unreal.EditorLevelLibrary.spawn_actor_from_class(
   unreal.StaticMeshActor,unreal.Vector(center.x,center.y,center.z+18000))
  if not isinstance(block,unreal.StaticMeshActor):
   raise RuntimeError("瞬态Cube未创建")
  mesh=block.get_editor_property("static_mesh_component")
  mesh.set_static_mesh(cube)
  mesh.set_material(0,default)
  block.set_actor_scale3d(unreal.Vector(420,420,420))
  capture("GridPlusCube")
 finally:
  if block:
   unreal.EditorLevelLibrary.destroy_actor(block)
  if target.get_editor_property("landscape_material")!=old:
   target.set_editor_property("landscape_material",old)
  if cap:
   unreal.EditorLevelLibrary.destroy_actor(cap)
  # 此脚本不保存任何UWorld及其临时修改，文件身份始终归项目QA所有者。
  print("SSLM_V3_MATERIAL_ISOLATION_WORLD_RESTORED no_map_save=true",flush=True)
 (OUT/"IsolationManifest.json").write_text(json.dumps({
  "map":MAP,"originalMaterial":old.get_path_name(),
  "temporaryEngineGrid":known.get_path_name(),"temporaryEngineCube":cube.get_path_name(),
  "frames":photos,"pixelQaPassed":False,"humanReviewPassed":False,
  "noteZh":"仅证明UE原生试验设置，真实像素在独立检查中决定"
 },ensure_ascii=False,indent=2)+"\n",encoding="utf8")
 print("SSLM_V3_MATERIAL_ISOLATION_NATIVE_DONE frames="+str(len(photos))+" pixel_qa_pending=true",flush=True)

if __name__=="__main__":
 run()
