#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7/P8两张现有审核图的可视化检视相机补充，真实UE5.8创建并保存。

P7P9_CAMERA_TASK=heroes 或 sfx；P7P9_CAMERA_MODE=inspect（只读，缺失失败）
或 apply（只在无任何本工具相机时批量创建；拒绝覆盖或自动修复部分遗留资产）。
每个编辑器进程只加载一张地图，避免在加载旧UWorld后保留其引用导致GC泄漏。
相机只用于编辑器Pilot/移动端预览，不自动接管游戏摄像机，不改变玩法或声音。
"""
from __future__ import annotations
import json
import math
import os
from pathlib import Path
import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
BASE="/Game/Development/Foundation/PluginReviews/Maps"
OUT=ROOT/"Saved/Validation/PluginP7P9_20261012"
HEROES={
    "Rat":"子鼠","Ox":"丑牛","Tiger":"寅虎","Rabbit":"卯兔",
    "Dragon":"辰龙","Snake":"巳蛇","Horse":"午马","Goat":"未羊",
    "Monkey":"申猴","Rooster":"酉鸡","Dog":"戌狗","Boar":"亥猪",
}
SFX={"TwoD":"二维","World":"三维空间","Attached":"附着"}
TASK=os.getenv("P7P9_CAMERA_TASK","").lower()
MODE=os.getenv("P7P9_CAMERA_MODE","inspect").lower()
SCENES={
    "heroes":("L_DBA_HeroShowcase","HeroShowcase_","ReviewHeroCloseup_",HEROES),
    "sfx":("L_GP_SFXReview","SFXReview_","ReviewSFXCloseup_",SFX),
}

def check_environment():
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("仅允许锁定项目使用")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("UE编辑器版本必须为5.8")
    if TASK not in SCENES or MODE not in ("inspect","apply"):
        raise RuntimeError("必须指定P7P9_CAMERA_TASK=heroes/sfx及MODE=inspect/apply")
    config=(ROOT/"Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if 'DirectoriesToNeverCook=(Path="/Game/Development/Foundation")' not in config:
        raise RuntimeError("审核资产不具备明确发布排除")


def main():
    check_environment()
    map_name,stage_prefix,camera_prefix,roles=SCENES[TASK]
    map_path=BASE+"/"+map_name
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path) or not level.load_level(map_path):
        raise RuntimeError("地图无法在独立UE编辑器装载："+map_path)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing=list(actors.get_all_level_actors())
    by_label={a.get_actor_label():a for a in existing}
    required={stage_prefix+identity:identity for identity in roles}
    missing=[x for x in required if x not in by_label]
    if missing:
        raise RuntimeError("正式演示站缺失："+", ".join(missing))
    already=[label for label in by_label if label.startswith(camera_prefix)]
    desired={camera_prefix+ident for ident in roles}
    if already and (set(already)!=desired or len(already)!=len(desired)):
        raise RuntimeError("已存在部分或异常近景相机，禁止自动覆盖："+str(already))
    if MODE=="inspect" and set(already)!=desired:
        raise RuntimeError("真实UE审核图缺少近景相机：" + str(sorted(desired-set(already))))
    if MODE=="apply" and not already:
        for identity,cn_name in roles.items():
            stage=by_label[stage_prefix+identity]
            pos=stage.get_actor_location()
            # 现有平台PreviewStage已将标准Manny/Quinn模型放在舞台原点附近。
            # 三维空间里相机距主体约4.8m，适合看材质、骨骼与轮廓；位置为编辑器预览，不产生新Actor权威。
            cam_pos=unreal.Vector(pos.x, pos.y-480.0,pos.z+230.0)
            target=unreal.Vector(pos.x,pos.y,pos.z+70.0)
            delta=target-cam_pos
            horizontal=math.hypot(delta.x,delta.y)
            rotation=unreal.Rotator(
                pitch=math.degrees(math.atan2(delta.z,horizontal)),
                yaw=math.degrees(math.atan2(delta.y,delta.x)),
                roll=0.0)
            camera=actors.spawn_actor_from_class(unreal.CameraActor,cam_pos,rotation)
            if camera is None:
                raise RuntimeError("无法通过真正UE系统创建相机："+identity)
            camera.set_actor_label(camera_prefix+identity)
            component=camera.get_component_by_class(unreal.CameraComponent)
            if component is None:
                raise RuntimeError("CameraActor没有真实相机组件："+identity)
            component.set_field_of_view(54.0)
            print("P7P9_REVIEW_CLOSEUP_CAMERA_SAVED",cn_name,identity,component.field_of_view)
        if not level.save_current_level():
            raise RuntimeError("UE审核图保存失败："+map_path)
    else:
        for name in desired:
            if not isinstance(by_label.get(name),unreal.CameraActor):
                raise RuntimeError("近景对象不是真正CameraActor："+name)
    final_labels={a.get_actor_label() for a in actors.get_all_level_actors()}
    missing=desired-final_labels
    if missing:
        raise RuntimeError("执行后近景相机仍未全部存在："+str(sorted(missing)))
    report={
        "status":"passed","mode":MODE,"task":TASK,
        "engine":str(unreal.SystemLibrary.get_engine_version()),
        "map":map_path,
        "count":len(desired),
        "labels":[{"name":camera_prefix+k,"chinese":v} for k,v in roles.items()],
        "manualReview":"pending",
        "scope":"仅提升独立检查视角，不是最终3A美术或听觉签收",
    }
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/("ReviewCameras_"+TASK+"_"+MODE+".json")).write_text(
        json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("P7P9_REVIEW_CLOSEUP_CAMERA_ALL_OK",TASK,len(desired),MODE)

if __name__=="__main__":
    main()
