# -*- coding: utf-8 -*-
"""
神兽联盟十二生肖原型角色资产生成器。
仅用于开发期资产生成，不在运行时执行。
"""

import json
import os
import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
MANIFEST_PATH = os.path.join(SCRIPT_DIR, "ZodiacPlaceholderCharacters.json")
TEMP_DBA_MANNEQUIN_ROOT = "/Game/DBA/Characters/Mannequins"
TEMP_STANDARD_MANNEQUIN_ROOT = "/Game/Characters/Mannequins"
COMMON_DBA_MANNEQUIN_ROOT = "/DBAContentPack_Common/Mannequins/DBA"
COMMON_STANDARD_MANNEQUIN_ROOT = "/DBAContentPack_Common/Mannequins/Standard"
COMMON_MATERIAL_ROOT = "/DBAContentPack_Common/Materials"
MASTER_MATERIAL_PATH = COMMON_MATERIAL_ROOT + "/M_DBA_PrototypeCharacterColor"
HERO_DEFINITION_ROOT = "/DBAGameplay/Definitions"
TINT_PARAMETER = "PrototypeTint"


def log(message):
    unreal.log("[DBA Zodiac Prototype] " + str(message))


def fail(message):
    raise RuntimeError("[DBA Zodiac Prototype] " + str(message))


def load_manifest():
    with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def get_or_create_asset(asset_name, package_path, asset_class, factory):
    object_path = package_path + "/" + asset_name
    existing = unreal.EditorAssetLibrary.load_asset(object_path)
    if existing:
        return existing
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, asset_class, factory
    )
    if not asset:
        fail("创建资产失败: " + object_path)
    return asset


def migrate_assets(source_root, target_root, label):
    """逐资产迁移并让UE修复引用，避免UE5.8整目录rename_directory在混合依赖目录上失败。"""
    source_assets = unreal.EditorAssetLibrary.list_assets(
        source_root, recursive=True, include_folder=False
    )
    if not source_assets:
        fail("未发现待迁移资产: " + source_root)

    moved = 0
    failed = []
    for source_asset in source_assets:
        # list_assets可能返回ObjectPath（包名.对象名）；rename_asset要求资产路径。
        source_path = source_asset.split(".", 1)[0]
        relative = source_path[len(source_root):]
        target_path = target_root + relative

        if unreal.EditorAssetLibrary.does_asset_exist(target_path):
            unreal.EditorAssetLibrary.delete_asset(target_path)

        if unreal.EditorAssetLibrary.rename_asset(source_path, target_path):
            moved += 1
        else:
            failed.append(source_path + " -> " + target_path)

    if failed:
        fail(label + "逐资产迁移失败: " + "; ".join(failed))
    log(label + "迁移完成，共" + str(moved) + "个资产。")


def ensure_mannequin_mount():
    common_manny = COMMON_DBA_MANNEQUIN_ROOT + "/Meshes/SKM_Manny_Simple"
    common_quinn = COMMON_DBA_MANNEQUIN_ROOT + "/Meshes/SKM_Quinn_Simple"
    common_skeleton = "/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin"
    common_physics_asset = COMMON_DBA_MANNEQUIN_ROOT + "/Meshes/SK_Mannequin_PhysicsAsset"
    if (
        unreal.EditorAssetLibrary.does_asset_exist(common_manny)
        and unreal.EditorAssetLibrary.does_asset_exist(common_quinn)
        and unreal.EditorAssetLibrary.does_asset_exist(common_skeleton)
        and unreal.EditorAssetLibrary.does_asset_exist(common_physics_asset)
    ):
        # 文件存在只证明路径可解析；旧UE4骨架也含root/pelvis/head，必须检查UE5完整骨骼覆盖。
        skeleton = unreal.load_asset(common_skeleton)
        skeleton_names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(skeleton.get_reference_pose())}
        for mesh_path in (common_manny, common_quinn):
            mesh = unreal.load_asset(mesh_path)
            mesh_bones = ['root']
            for bone in mesh_bones:
                mesh_bones.extend(str(child) for child in mesh.get_bone_children(bone))
            if mesh.get_editor_property('skeleton') != skeleton or set(mesh_bones) - skeleton_names:
                fail('公共UE5骨架绑定错误，先通过Monolith修复，不重新迁移或删除现有资源: ' + mesh_path)
        log("公共Manny/Quinn、完整UE5骨架与PhysicsAsset已验证，跳过迁移。")
        return

    if unreal.EditorAssetLibrary.does_asset_exist(common_manny) or unreal.EditorAssetLibrary.does_asset_exist(common_quinn):
        fail('已有公共网格但UE5依赖不完整；先通过Monolith修复，禁止清空公共内容包后重迁移。')

    if unreal.EditorAssetLibrary.does_directory_exist('/DBAContentPack_Common/Mannequins'):
        fail('公共资源不完整，拒绝删除现有模板骨架/材质；从版本库恢复后用Monolith检查。')

    # 旧项目的Manny/Quinn网格位于/DBA/...，但默认材质继续引用标准
    # /Game/Characters/Mannequins。两个根必须同时进入编辑器后再迁移，
    # 由UE逐资产重命名修复跨挂载点引用，禁止在文件系统直接改名uasset。
    if not unreal.EditorAssetLibrary.does_directory_exist(TEMP_DBA_MANNEQUIN_ROOT):
        fail("缺少临时DBA Mannequin目录，请先运行PowerShell包装脚本。")
    if not unreal.EditorAssetLibrary.does_directory_exist(TEMP_STANDARD_MANNEQUIN_ROOT):
        fail("缺少临时标准Mannequin依赖目录，请先运行PowerShell包装脚本。")

    common_parent = "/DBAContentPack_Common/Mannequins"
    if unreal.EditorAssetLibrary.does_directory_exist(common_parent):
        unreal.EditorAssetLibrary.delete_directory(common_parent)

    log("逐资产迁移标准Mannequin材质/纹理依赖到公共内容包。")
    migrate_assets(
        TEMP_STANDARD_MANNEQUIN_ROOT,
        COMMON_STANDARD_MANNEQUIN_ROOT,
        "标准Mannequin依赖",
    )

    log("逐资产迁移Manny/Quinn网格与DBA材质到公共内容包。")
    migrate_assets(
        TEMP_DBA_MANNEQUIN_ROOT,
        COMMON_DBA_MANNEQUIN_ROOT,
        "DBA Mannequin资源",
    )

    if not unreal.EditorAssetLibrary.does_asset_exist(common_manny):
        fail("迁移后缺少SKM_Manny_Simple。")
    if not unreal.EditorAssetLibrary.does_asset_exist(common_quinn):
        fail("迁移后缺少SKM_Quinn_Simple。")
    if not unreal.EditorAssetLibrary.does_asset_exist(common_skeleton):
        fail("迁移后缺少完整UE5骨架SK_Mannequin，旧UE4骨架不满足Manny/Quinn依赖。")
    if not unreal.EditorAssetLibrary.does_asset_exist(common_physics_asset):
        fail("迁移后缺少SK_Mannequin_PhysicsAsset，Manny/Quinn依赖闭包不完整。")


def ensure_master_material():
    material = unreal.EditorAssetLibrary.load_asset(MASTER_MATERIAL_PATH)
    if material:
        # Cook不能推断动态覆盖用途，持久化骨骼网格着色器变体。
        material.set_editor_property("used_with_skeletal_mesh", True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
        return material

    material = get_or_create_asset(
        "M_DBA_PrototypeCharacterColor",
        COMMON_MATERIAL_ROOT,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )

    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -350, 0
    )
    tint.set_editor_property("parameter_name", TINT_PARAMETER)
    tint.set_editor_property("default_value", unreal.LinearColor(0.18, 0.45, 0.75, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        tint, "", unreal.MaterialProperty.MP_BASE_COLOR
    )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -350, 150
    )
    roughness.set_editor_property("r", 0.62)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    material.set_editor_property("used_with_skeletal_mesh", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def hex_to_color(value):
    value = value.lstrip("#")
    if len(value) != 6:
        fail("颜色格式错误: " + value)
    return unreal.LinearColor(
        int(value[0:2], 16) / 255.0,
        int(value[2:4], 16) / 255.0,
        int(value[4:6], 16) / 255.0,
        1.0,
    )


def enum_value(enum_type, suffix):
    for candidate in (suffix.upper(), suffix, suffix.lower()):
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    fail("无法解析生肖枚举: " + suffix)


def make_gameplay_tag(tag_name):
    # UE5.8 Python中的GameplayTag.TagName为只读属性，必须通过UStruct文本导入。
    tag = unreal.GameplayTag()
    if not tag.import_text(tag_name):
        fail("无法导入GameplayTag: " + tag_name)
    if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag):
        fail("GameplayTag未注册或无效: " + tag_name)
    return tag


def ensure_material_instance(hero, master_material):
    suffix = hero["heroId"].split(".")[-1]
    package_path = "/" + hero["pack"] + "/Characters/Materials"
    asset_name = "MI_Zodiac_" + suffix + "_Prototype"
    material = get_or_create_asset(
        asset_name,
        package_path,
        unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew(),
    )
    unreal.MaterialEditingLibrary.set_material_instance_parent(material, master_material)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        material, TINT_PARAMETER, hex_to_color(hero["colorHex"])
    )
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def ensure_appearance_profile(hero, material):
    suffix = hero["heroId"].split(".")[-1]
    package_path = "/" + hero["pack"] + "/Characters"
    asset_name = "DA_Appearance_Zodiac_" + suffix

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DivineBeastsCharacterAppearanceProfile)
    profile = get_or_create_asset(
        asset_name,
        package_path,
        unreal.DivineBeastsCharacterAppearanceProfile,
        factory,
    )

    mesh_name = "SKM_Manny_Simple" if hero["mesh"] == "Manny" else "SKM_Quinn_Simple"
    mesh = unreal.EditorAssetLibrary.load_asset(COMMON_DBA_MANNEQUIN_ROOT + "/Meshes/" + mesh_name)
    if not mesh:
        fail("缺少占位Mesh: " + mesh_name)

    # 旧项目ABP_Manny依赖Mover示例类型，在当前正式工程中会产生失效节点。
    # 原型角色只复用Mesh/Skeleton，动画留给GamePlatformAnimation后续统一接管。
    # 这样未来替换真实生肖模型时不把临时Mover依赖带入正式架构。

    profile.set_editor_property("profile_id", unreal.Name("Appearance.Hero.Zodiac." + suffix + ".Default"))
    profile.set_editor_property("hero_definition_id", unreal.Name(hero["heroId"]))
    profile.set_editor_property("skeleton_compatibility_id", unreal.Name("Skeleton.UE5.Mannequin"))
    profile.set_editor_property("skeletal_mesh", mesh)
    profile.set_editor_property("material_overrides", [material])
    profile.set_editor_property("anim_instance_class", None)
    profile.set_editor_property("version", 1)
    profile.set_editor_property("content_revision", "Prototype.MannequinColor.1")
    profile.set_editor_property("development_placeholder", True)
    profile.set_editor_property("development_tint", hex_to_color(hero["colorHex"]))
    profile.set_editor_property("mesh_relative_location", unreal.Vector(0.0, 0.0, -90.0))
    # Python参数为Roll/Pitch/Yaw，与C++不同；使用命名参数避免模型横倒。
    profile.set_editor_property("mesh_relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
    profile.set_editor_property("mesh_relative_scale", unreal.Vector(1.0, 1.0, 1.0))

    unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    return profile


def set_appearance_schema(definition):
    schema = definition.get_editor_property("appearance_schema")
    schema.set_editor_property("body_variants", [unreal.Name("Default")])
    schema.set_editor_property("head_presets", [unreal.Name("Default")])
    schema.set_editor_property("skin_marking_presets", [unreal.Name("None")])
    definition.set_editor_property("appearance_schema", schema)


def ensure_hero_definition(hero):
    suffix = hero["heroId"].split(".")[-1]
    asset_name = "DA_Hero_Zodiac_" + suffix

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DivineBeastsHeroDefinition)
    definition = get_or_create_asset(
        asset_name,
        HERO_DEFINITION_ROOT,
        unreal.DivineBeastsHeroDefinition,
        factory,
    )

    definition.set_editor_property("definition_id", unreal.Name(hero["heroId"]))
    definition.set_editor_property("version", 1)
    definition.set_editor_property("content_revision", "Prototype.CharacterDefinition.1")
    definition.set_editor_property("appearance_profile_id", unreal.Name("Appearance.Hero.Zodiac." + suffix + ".Default"))
    definition.set_editor_property("presentation_profile_id", unreal.Name("Presentation.Hero.Zodiac." + suffix + ".Default"))
    definition.set_editor_property("skeleton_compatibility_id", unreal.Name("Skeleton.UE5.Mannequin"))
    definition.set_editor_property("zodiac_identity", enum_value(unreal.DivineBeastsZodiacIdentity, suffix))
    definition.set_editor_property("zodiac_tag", make_gameplay_tag("DBA.Character.Zodiac." + suffix))
    definition.set_editor_property("display_name_key", unreal.Name(hero["heroId"] + ".Name"))
    definition.set_editor_property("content_pack_id", unreal.Name("ContentPack.Hero.Zodiac." + suffix))
    set_appearance_schema(definition)

    unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False)
    return definition


def validate_generated_assets(manifest):
    missing = []
    for hero in manifest["heroes"]:
        suffix = hero["heroId"].split(".")[-1]
        pack = hero["pack"]
        expected = [
            HERO_DEFINITION_ROOT + "/DA_Hero_Zodiac_" + suffix,
            "/" + pack + "/Characters/DA_Appearance_Zodiac_" + suffix,
            "/" + pack + "/Characters/Materials/MI_Zodiac_" + suffix + "_Prototype",
        ]
        for path in expected:
            if not unreal.EditorAssetLibrary.does_asset_exist(path):
                missing.append(path)
    if missing:
        fail("以下生成资产缺失: " + ", ".join(missing))
    # 文件存在不能证明迁移引用完整；禁止再次把断骨架或旋转错误标记为生成成功。
    for hero in manifest["heroes"]:
        suffix = hero["heroId"].split(".")[-1]
        profile = unreal.load_asset("/" + hero["pack"] + "/Characters/DA_Appearance_Zodiac_" + suffix)
        rotation = profile.get_editor_property("mesh_relative_rotation")
        mesh = profile.get_editor_property("skeletal_mesh")
        if not mesh or not mesh.get_editor_property("skeleton") or abs(rotation.pitch) > 0.01 or abs(rotation.yaw + 90.0) > 0.01:
            fail("外观骨架/旋转校验失败，先通过Monolith恢复既定引用: " + suffix)


def main():
    manifest = load_manifest()
    if len(manifest.get("heroes", [])) != 12:
        fail("清单必须恰好包含12个生肖。")

    ensure_mannequin_mount()
    master = ensure_master_material()

    for hero in manifest["heroes"]:
        material = ensure_material_instance(hero, master)
        ensure_appearance_profile(hero, material)
        ensure_hero_definition(hero)
        log("已生成/更新: " + hero["zodiacCn"] + " " + hero["displayNameCn"] + " / " + hero["heroId"])

    validate_generated_assets(manifest)
    unreal.EditorAssetLibrary.save_directory("/DBAContentPack_Common", only_if_is_dirty=False, recursive=True)
    unreal.EditorAssetLibrary.save_directory("/DBAGameplay/Definitions", only_if_is_dirty=False, recursive=True)
    for hero in manifest["heroes"]:
        unreal.EditorAssetLibrary.save_directory("/" + hero["pack"], only_if_is_dirty=False, recursive=True)

    log("十二生肖原型角色资产生成完成。")


if __name__ == "__main__":
    main()
