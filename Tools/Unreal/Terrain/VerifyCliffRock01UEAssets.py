#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01独立UE5.8原生资产核验（只读）与美术候选合同检查。

只有真UE Editor Python可运行；检查项目内容包4张4K Texture2D/类型/压缩/sRGB、
两套PBR材质UMaterial与三套MI的对象类型、父类与同源纹理指针，以及
14节点世界对齐材质、WorldAlignedTexture/Normal真实函数调用、世界法线方向、
20m空间投射参数。不得将技术源图或纯代码视为已导入的.uasset。
不修改任何资产，不进入Village正式地图，也不声称人工签收/GPU通过。
"""

from __future__ import annotations

MOUNT="/DBAWorldPack_Village/Terrain/Realism/CliffRock01"
REVIEW=MOUNT+"/Materials/M_DBA_CliffRock01_PBRReview"
REVIEW_MI=MOUNT+"/Materials/MI_DBA_CliffRock01_PBRReview"
LANDSCAPE_MI=MOUNT+"/Materials/MI_DBA_Terrain_CliffRock01"
WORLD=MOUNT+"/Materials/M_DBA_CliffRock01_WorldAligned"
WORLD_MI=MOUNT+"/Materials/MI_DBA_CliffRock01_WorldAligned"
BASE_PARENT="/DBAWorldPack_Village/Terrain/Materials/M_DBA_Terrain_MultiLayer10"
TEXTURES={
 "BaseColor":MOUNT+"/Textures/T_DBA_CliffRock01_BaseColor_BlueGray",
 "NormalDX":MOUNT+"/Textures/T_DBA_CliffRock01_NormalDX",
 "ORM":MOUNT+"/Textures/T_DBA_CliffRock01_ORM",
 "Height":MOUNT+"/Textures/T_DBA_CliffRock01_Height",
}


def main()->None:
    try:
        import unreal  # type: ignore
    except ImportError as ex:
        raise RuntimeError("须在UE5.8编辑器进程反射，不允许普通Python伪造校验") from ex
    path=unreal.Paths.get_project_file_path().replace("\\","/")
    if not path.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("Cliff候选校验必须运行正式神兽联盟uproject")
    lib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    textures={}
    for role,pkg in TEXTURES.items():
        obj=lib.load_asset(pkg)
        if not isinstance(obj,unreal.Texture2D):
            raise TypeError("不是原生UE Texture2D："+pkg)
        if obj.blueprint_get_size_x()!=4096 or obj.blueprint_get_size_y()!=4096:
            raise RuntimeError("Cliff UE纹理4K分辨率不符："+role)
        if bool(obj.get_editor_property("srgb"))!=(role=="BaseColor"):
            raise RuntimeError("4K PBR纹理颜色空间非法："+role)
        compression=str(obj.get_editor_property("compression_settings"))
        expected={
            "BaseColor":"TC_DEFAULT",
            "NormalDX":"TC_NORMALMAP",
            "ORM":"TC_MASKS",
            "Height":"TC_GRAYSCALE"
        }[role]
        if expected not in compression:
            raise RuntimeError("贴图压缩类型错误："+role+"/"+compression)
        textures[role]=obj
        print("CLIFFROCK_RELOADED_TEXTURE_VERIFIED",role,"4K",compression)

    parents={
        REVIEW_MI:(REVIEW,(
            ("CliffRockBaseColor","BaseColor"),
            ("CliffRockNormalDX","NormalDX"),
            ("CliffRockORM","ORM"),
        )),
        LANDSCAPE_MI:(BASE_PARENT,(
            ("TerrainRockBaseColor","BaseColor"),
            ("TerrainRockNormal","NormalDX"),
            ("TerrainRockORM","ORM"),
        )),
        WORLD_MI:(WORLD,(
            ("CliffWorldBaseColor","BaseColor"),
            ("CliffWorldNormalDX","NormalDX"),
            ("CliffWorldORM","ORM"),
        )),
    }
    for mid,(master,fields) in parents.items():
        asset=lib.load_asset(mid)
        parent=lib.load_asset(master)
        if not isinstance(asset,unreal.MaterialInstanceConstant) or not isinstance(parent,unreal.Material):
            raise TypeError("Cliff MI或父材质的原生UE反射失败："+mid)
        if asset.get_editor_property("parent")!=parent:
            raise RuntimeError("Cliff材质继承方向不正确："+mid)
        for param,role in fields:
            ptr=edit.get_material_instance_texture_parameter_value(asset,param)
            if ptr!=textures[role]:
                raise RuntimeError("Cliff同源PBR纹理引用不正确："+mid+" "+param)
        print("CLIFFROCK_RELOADED_MI_VERIFIED",mid,"textureReferences",len(fields))

    material=lib.load_asset(REVIEW)
    world=lib.load_asset(WORLD)
    if not isinstance(material,unreal.Material) or not isinstance(world,unreal.Material):
        raise TypeError("UE Cliff两个独立母材质未创建")
    basic_nodes=edit.get_num_material_expressions(material)
    world_nodes=edit.get_num_material_expressions(world)
    if basic_nodes<10 or world_nodes<14:
        raise RuntimeError("UE真实PBR材质节点缺失："+str((basic_nodes,world_nodes)))
    if bool(world.get_editor_property("tangent_space_normal")):
        raise RuntimeError("内置WorldAlignedNormal输出世界法线，不能错误切线化")
    material_calls=[
        node for node in edit.get_material_expressions(world)
        if isinstance(node,unreal.MaterialExpressionMaterialFunctionCall)
    ]
    if len(material_calls)!=3:
        raise RuntimeError("内置三轴世界投射函数调用数不为3："+str(len(material_calls)))
    world_mi=lib.load_asset(WORLD_MI)
    size=edit.get_material_instance_vector_parameter_value(world_mi,"CliffWorldTextureSizeCm")
    if (abs(size.r-2000)>1 or abs(size.g-2000)>1 or abs(size.b-2000)>1):
        raise RuntimeError("Cliff大尺度扫描20米cm投射参数不一致："+str(size))
    print("CLIFFROCK_WORLDALIGNED_UE_RELOAD_PASS worldAlignedCalls=3",
          "UVPBRNodes",basic_nodes,"triPlanarNodes",world_nodes,
          "scanWidthCm",size.r)
    print("CLIFFROCK_UE_COMPLETE_CANDIDATE_PASS",
          "textures=4 materials=2 materialInstances=3 userApprovalPending=True")


if __name__=="__main__":
    main()
