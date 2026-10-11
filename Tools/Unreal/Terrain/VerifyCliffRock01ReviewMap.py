#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01真实20m独立崖壁场景的UE5.8重启回读（只读）。

本工具在UnrealEditor-Cmd执行。加载开发地图L_DBA_CliffRock01Review（项目正式
Village未接触），核实3个验证几何Actor、世界对齐20m材质、UV对照20m、2m标定、
三光源/天空及摄影机，并确认模型实际尺寸与材质绑定。
退出码与引擎UWorld/UObject反射是唯一有效地图创建证据；文件名存在不代表审查通过。
本脚本不修改地图，不能将其结果冒充GPU截图和人工最终签收。
"""

MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01Review"
MATS={
    "CliffReview_WorldAligned_20m":
        "/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAligned",
    "CliffReview_UV_20m":
        "/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_PBRReview",
    "CliffReview_WorldAligned_2m":
        "/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAligned_Review2m",
}


def main():
    try:import unreal # type: ignore
    except ImportError as ex:
        raise RuntimeError("必须由UE5.8真实编辑器运行地图反射核验") from ex
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE当前不在神兽联盟正式工程")
    assetlib=unreal.EditorAssetLibrary
    if not assetlib.does_asset_exist(MAP):
        raise FileNotFoundError("实际UE地图不存在："+MAP)
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("UE5.8没有实际打开独立崖壁审核地图")
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise TypeError("打开关卡的UWorld对象身份不正确")
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    by_label={a.get_actor_label():a for a in actors}
    if len(by_label)!=len(actors):
        raise RuntimeError("存在重复ActorLabel，审查图不可追溯")
    for label,material_path in MATS.items():
        actor=by_label.get(label)
        if not isinstance(actor,unreal.StaticMeshActor):
            raise TypeError("审核样本没有真实立面网格："+label)
        mat=assetlib.load_asset(material_path)
        if not isinstance(mat,unreal.MaterialInstanceConstant):
            raise TypeError("引用的Cliff测试材质缺失："+material_path)
        comp=actor.get_editor_property("static_mesh_component")
        if comp.get_material(0)!=mat:
            raise RuntimeError("真实UE立面实际绑定的不是期望Cliff纹理："+label)
        scale=actor.get_actor_scale3d()
        if label.endswith("_20m"):
            if abs(scale.y-19.5)>.05 or abs(scale.z-13)>.05:
                raise RuntimeError("20m测试立面物理尺寸不匹配："+label)
        else:
            if abs(scale.y-2)>.05:
                raise RuntimeError("2米纹理尺度比较方块尺寸不正确")
        print("CLIFFROCK_REVIEW_MAP_UE_ACTOR_VERIFIED",label,mat.get_path_name(),
              "scale",round(scale.x,2),round(scale.y,2),round(scale.z,2))
    for light_label in (
        "CliffReview_Sun","CliffReview_LeftSoftbox","CliffReview_RightSoftbox",
        "CliffReview_Skylight","CliffReview_SkyAtmosphere","CliffReview_Camera_20m"):
        if light_label not in by_label:
            raise RuntimeError("缺少可控照明或真实审查相机："+light_label)
    camera=by_label["CliffReview_Camera_20m"]
    if not isinstance(camera,unreal.CameraActor):
        raise TypeError("20米崖壁相机对象类别非法")
    print("CLIFFROCK_REVIEW_20M_UWORLD_RELOAD_PASS",MAP,
          "walls=2 qa=1 lights=4 sky=True camera=True")


if __name__=="__main__":
    main()
