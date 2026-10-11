# -*- coding: utf-8 -*-
"""只修改现有Niagara开发审核地图的粒子预热，保留源关卡、Actor身份和图形资源。
仅供人工VFX审核，不能把成功保存当作角色工作室动态效果通过。
"""
import unreal
from pathlib import Path
import shutil
root=Path(unreal.Paths.project_dir()).resolve().parent
path="/Game/Development/FallingFoliage/Maps/L_DBA_FallingFoliageReview"
uasset=root/"Game/Content/Development/FallingFoliage/Maps/L_DBA_FallingFoliageReview.umap"
backup=root/"Saved/Validation/FallingFoliage/BeautyV2UEBackup/ReviewMap/L_DBA_FallingFoliageReview.umap"
project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
if not project.endswith("/DivineBeastsArena.uproject") or not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
    raise RuntimeError("必须使用正式神兽联盟UE5.8")
if not uasset.exists():raise FileNotFoundError(str(uasset))
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(uasset,backup)
world=unreal.EditorLoadingAndSavingUtils.load_map(path)
if not world: raise RuntimeError("无法读取真实审核关卡")
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
matches=[a for a in actors if a.get_actor_label()=="FoliageReview_PeachNiagara"]
if len(matches)!=1:raise RuntimeError("审核地图NiagaraActor数量不等于1")
actor=matches[0]
comp=actor.get_component_by_class(unreal.NiagaraComponent)
if not comp or not comp.get_asset():raise RuntimeError("审核地图Niagara组件缺失")
if not comp.get_asset().get_path_name().split(".")[0].endswith("/NS_DBA_FallingFoliage_Peach"):
    raise RuntimeError("当前Niagara组件指向非桃花资产")
# 让GPU发射器在审核帧之前模拟约5秒，避免此前首帧空画面误判。
comp.set_editor_property("override_warmup_settings",True)
comp.set_editor_property("warmup_tick_count",150)
comp.set_editor_property("warmup_tick_delta",1/30)
comp.set_editor_property("auto_activate",True)
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("真实审核地图保存失败")
print("BEAUTY_V2_NIAGARA_REVIEW_WARMUP_SAVED",path,
      comp.get_editor_property("override_warmup_settings"),
      comp.get_editor_property("warmup_tick_count"),flush=True)
