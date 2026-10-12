# -*- coding: utf-8 -*-
"""通过Monolith editor.run_python在已有角色工作室增量铺设少量静态叶片。

第三层DBAFrontEndPack拥有场景Actor，叶片材质复用DBAPresentationPack_Core；无Tick、
无碰撞、无复制，不改变角色、地形、相机或PlayerStart。只接受正确地图和无未保存包。
修改前备份地图及外置Actor，失败不宣称成功；重跑遇到已有本轮标签即拒绝重复铺设。
"""
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import unreal

STUDIO = "/DBAFrontEndPack/Maps/L_DBA_CharacterStudio"
TAG = "DBA.FrontEnd.GroundCover"


def author():
    """在已批准圆台石面上铺40片叶，固定种子仅用于离线美术摆放而非跨端权威。"""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_path_name().split(".")[0] != STUDIO:
        raise RuntimeError("必须先用Monolith打开既有角色工作室")
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
        raise RuntimeError("有未保存修改，拒绝覆盖")
    api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = api.get_all_level_actors()
    if any(a.actor_has_tag(TAG) for a in actors):
        raise RuntimeError("已有落叶层，须只读审核后增量修改，不重复生成")
    stage = next(a for a in actors if a.get_actor_label() == "CharacterPreviewStage")
    architecture = next(a for a in actors if a.get_actor_label() == "SM_DBA_Sanctuary_Architecture")
    scale = architecture.get_actor_scale3d()
    if any(abs(value - .45) > .0001 for value in (scale.x, scale.y, scale.z)):
        raise RuntimeError("圣境缩放已变，原石面标高不再适用")
    starts = {a.get_path_name(): str(a.get_actor_transform()) for a in actors if isinstance(a, unreal.PlayerStart)}
    root = Path(unreal.Paths.project_dir()).resolve().parent
    pack = root / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/Content"
    backup = root / "Saved/Validation/CharacterEntryFollowUp20261012/BeforeGroundCover"
    if backup.exists():
        raise RuntimeError("本轮备份已经存在，拒绝覆盖回退证据")
    backup.mkdir(parents=True)
    files = [pack / "Maps/L_DBA_CharacterStudio.umap"]
    for category in ("__ExternalActors__", "__ExternalObjects__"):
        folder = pack / category / "Maps/L_DBA_CharacterStudio"
        if folder.exists():
            files += list(folder.rglob("*.*"))
    evidence = {"Map": STUDIO, "PlayerStartsBefore": starts, "Backups": [], "Leaves": []}
    for source in files:
        target = backup / source.relative_to(pack)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        evidence["Backups"].append({"Path": str(source), "SHA256": hashlib.sha256(source.read_bytes()).hexdigest()})
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    materials = [unreal.load_asset("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Materials/MI_DBA_Foliage_" + name) for name in ("Bamboo", "Ginkgo")]
    if not plane or not all(materials):
        raise RuntimeError("已有叶片网格/材质缺失")
    rng = random.Random(20261012)
    origin = stage.get_actor_location()
    # 原Blender圆台石面顶标高0.02m，45%场景缩放后为0.9cm；增加0.15cm避免共面。
    # 半径140~248cm避开中心角色和金色圆环；不使用隐藏Landscape的0cm碰撞替代可见石面。
    for index in range(40):
        angle = rng.uniform(0, math.tau)
        radius = math.sqrt(rng.uniform(140**2, 248**2))
        # 叶片半宽最大5cm，距离饰带至少8cm，避免铺在凸起的雕刻上。
        while any(abs(radius - ring) < 8 for ring in (155, 160, 205)):
            radius = math.sqrt(rng.uniform(140**2, 248**2))
        position = unreal.Vector(origin.x + radius * math.cos(angle), origin.y + radius * math.sin(angle), origin.z + 1.05)
        leaf = api.spawn_actor_from_class(unreal.StaticMeshActor, position)
        leaf.set_actor_label("CharacterStudioGroundLeaf_{:02d}".format(index))
        leaf.set_folder_path("ZodiacSanctuary/GroundCover")
        leaf.tags = [unreal.Name(TAG)]
        leaf.set_actor_rotation(unreal.Rotator(pitch=0, yaw=rng.uniform(0, 360), roll=0), False)
        size = rng.uniform(.055, .10)
        leaf.set_actor_scale3d(unreal.Vector(size, size, size))
        component = leaf.static_mesh_component
        component.set_static_mesh(plane)
        component.set_material(0, materials[index % 2])
        # 只调用运行时SetCollisionEnabled会在关卡重载时被BlockAll配置还原；
        # 同时持久化NoCollision配置和Actor碰撞开关，保证装饰不会改变地表查询。
        component.set_collision_profile_name("NoCollision")
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        leaf.set_actor_enable_collision(False)
        component.set_editor_property("cast_shadow", False)
        leaf.set_replicates(False)
        evidence["Leaves"].append({"Name": leaf.get_actor_label(), "LocationCm": [position.x, position.y, position.z], "SizeCm": 100 * size})
    after = {a.get_path_name(): str(a.get_actor_transform()) for a in api.get_all_level_actors() if isinstance(a, unreal.PlayerStart)}
    if starts != after:
        raise RuntimeError("PlayerStart发生非预期变化，拒绝保存")
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("真实地图保存失败")
    evidence["PlayerStartsPreserved"] = True
    evidence["Saved"] = True
    (backup.parent / "GroundCoverAuthoring.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding="utf-8")
    print("CHARACTER_STUDIO_GROUND_COVER_SAVED", len(evidence["Leaves"]))


author()
