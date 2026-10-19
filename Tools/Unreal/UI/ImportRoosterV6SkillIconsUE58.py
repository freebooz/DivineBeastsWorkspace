# -*- coding: utf-8 -*-
"""SSLM 酉鸡 V6 图标原生UE5.8导入与唯一UIProfile增补。
使用Monolith editor.run_python调用，默认inspect只读；需显式设置ROOSTER_V6_MODE。
不碰服务器GAS/AbilitySet、不覆盖V1源图、不创建第二份主资产。
"""
from pathlib import Path
import hashlib, json, os, struct, shutil, zipfile

WORKSPACE = Path(__file__).resolve().parents[3]
PLAN = WORKSPACE / "Docs/Implementation/GamePlatformAbilitySystem/RoosterV6SkillIconImportManifest_20261019.json"
MODE = str(globals().get("ROOSTER_V6_MODE", os.environ.get("SSLM_ROOSTER_V6_MODE", "inspect"))).lower().strip()
LOG_DIR = WORKSPACE / "Game/Saved/Validation/RoosterV6IconImport"
PACK = WORKSPACE / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rooster"
SOURCE = PACK / "SourceArt/UI/SkillIcons/V6"
ZIP_LIMIT = 120 * 1024 * 1024

def require(ok, msg):
    if not ok:
        raise RuntimeError("SSLM酉鸡V6资源门禁：" + str(msg))

def sha(blob):
    return hashlib.sha256(blob).hexdigest()

def dimensions(blob):
    require(len(blob) > 24 and blob[:8] == b"\x89PNG\r\n\x1a\n", "仅允许有效PNG")
    return struct.unpack(">II", blob[16:24])

def load_inputs():
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    require(plan["heroId"] == "Hero.Zodiac.Rooster" and len(plan["entries"]) == 8, "技能清单不完整")
    ability_ids = [r["abilityId"] for r in plan["entries"]]
    require(len(ability_ids) == len(set(ability_ids)), "AbilityId重复")
    require([r["order"] for r in plan["entries"]] == list(range(1, 9)), "技能顺序不连续")
    required_slots = ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"]
    require([r["slotId"] for r in plan["entries"]] == required_slots, "八槽顺序不匹配")
    filename = WORKSPACE / plan["zipRelativePath"]
    if not filename.is_file():
        print("SSLM_ROOSTER_V6_WAITING_ART_ZIP:", filename)
        return plan, None, {}
    require(filename.stat().st_size < ZIP_LIMIT, "图片包过大")
    require(sha(filename.read_bytes()) == plan["zipSha256"], "ZIP包字节SHA-256不匹配")
    image_bytes = {}
    with zipfile.ZipFile(filename) as z:
        members = set(z.namelist())
        for entry in plan["entries"]:
            for field, shape in (("runtime48", (48,48)), ("review64", (64,64)), ("master", (1254,1254))):
                member = entry[field]
                require(member in members and not member.startswith("/") and ".." not in Path(member).parts, "缺文件或路径越界：" + member)
                blob = z.read(member)
                require(dimensions(blob) == shape, "PNG尺寸不匹配：" + member)
                if field == "runtime48":
                    require(sha(blob) == entry["sha256_48"], "正式48纹理SHA不匹配：" + member)
                image_bytes[member] = blob
    print("SSLM_ROOSTER_V6_SOURCE_PREFLIGHT_OK", len(image_bytes))
    return plan, filename, image_bytes

def check_editor():
    import unreal
    require(Path(unreal.Paths.get_project_file_path()).resolve() == (WORKSPACE / "Game/DivineBeastsArena.uproject").resolve(), "必须是正式SSLM项目")
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."), "必须是UE5.8")
    return unreal

def store_source(plan, image_bytes):
    """保存源图；旧同名源若不同字节则拒绝，不静默改写。"""
    for row in plan["entries"]:
        for field, subdir in (("master", "Masters"), ("review64", "Review64"), ("runtime48", "Runtime48")):
            relative = row[field]
            dst = SOURCE / subdir / Path(relative).name
            blob = image_bytes[relative]
            if dst.is_file():
                require(sha(dst.read_bytes()) == sha(blob), "工程已有异版本图禁止覆盖：" + str(dst))
            else:
                dst.parent.mkdir(parents=True, exist_ok=True)
                dst.write_bytes(blob)

def import_textures(unreal, plan):
    require(MODE == "import", "导入模式不符")
    asset_library = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for row in plan["entries"]:
        package_name = row["texturePath"]
        folder, short = package_name.rsplit("/", 1)
        runtime_png = SOURCE / "Runtime48" / (short + ".png")
        require(runtime_png.is_file(), "源图不存在：" + str(runtime_png))
        require(sha(runtime_png.read_bytes()) == row["sha256_48"], "当前运行源图已变动")
        if asset_library.does_asset_exist(package_name):
            asset = asset_library.load_asset(package_name)
            require(isinstance(asset, unreal.Texture2D), "已有同名非Texture2D资源")
        else:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(runtime_png))
            task.set_editor_property("destination_path", folder)
            task.set_editor_property("destination_name", short)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            tools.import_asset_tasks([task])
            asset = asset_library.load_asset(package_name)
            require(isinstance(asset, unreal.Texture2D), "真正UE Texture2D导入失败：" + package_name)
        asset.set_editor_property("srgb", True)
        asset.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        require(asset_library.save_loaded_asset(asset, only_if_is_dirty=False), "纹理保存失败：" + package_name)
        tag = str(asset_library.find_asset_data(package_name).get_tag_value("Dimensions"))
        require(tag in ("48x48", "48X48"), "真实纹理应当为48x48：" + package_name + "=" + tag)
    print("SSLM_ROOSTER_V6_TEXTURES_IMPORTED", len(plan["entries"]))

def inspect_single_profile(unreal, plan):
    lib = unreal.EditorAssetLibrary
    profile_path = plan["profilePath"]
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.scan_paths_synchronous(["/DBAHeroPack_Rooster/UI/Abilities"], True)
    assets = reg.get_assets_by_path("/DBAHeroPack_Rooster/UI/Abilities", recursive=True)
    matches = []
    for data in assets:
        if str(data.asset_class_path.asset_name) == "DivineBeastsAbilityUIProfile":
            candidate = lib.load_asset(str(data.package_name))
            if str(candidate.get_editor_property("hero_definition_id")) == plan["heroId"]:
                matches.append(str(data.package_name))
    require(matches == [profile_path], "同英雄主资产不唯一或现有Profile未注册：" + str(matches))
    profile = lib.load_asset(profile_path)
    require(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), "需复用现有唯一UIProfile")
    return profile

def snapshot_entry(entry):
    icon = entry.get_editor_property("icon")
    path = icon.get_path_name().split(".")[0] if icon else ""
    return {"id": str(entry.get_editor_property("ability_id")),
            "name": str(entry.get_editor_property("display_name")),
            "description": str(entry.get_editor_property("description")),
            "icon": path}

def bind_profile(unreal, plan):
    """保留旧条目；ID匹配但图或文案冲突直接停止。"""
    lib = unreal.EditorAssetLibrary
    profile = inspect_single_profile(unreal, plan)
    entries = list(profile.get_editor_property("entries"))
    old = [snapshot_entry(e) for e in entries]
    require(len({x["id"] for x in old}) == len(old), "旧Profile有重复技能ID")
    existing = {x["id"]: x for x in old}
    missing = []
    for row in plan["entries"]:
        texture = lib.load_asset(row["texturePath"])
        require(isinstance(texture, unreal.Texture2D), "先执行import创建真纹理：" + row["texturePath"])
        old_row = existing.get(row["abilityId"])
        if old_row is not None:
            require(old_row["name"] == row["displayNameZh"] and
                    old_row["icon"] == row["texturePath"] and old_row["description"].strip(),
                    "已有同ID条目与V6不一致，禁止覆盖：" + row["abilityId"])
        else:
            missing.append(row)
    if not missing:
        print("SSLM_ROOSTER_V6_PROFILE_ALREADY_MATCHES")
        return
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    snapshot = LOG_DIR / "UIProfileOriginalEntries.json"
    backup = LOG_DIR / "DA_DBA_Rooster_PlaceholderUIProfile_beforeV6.uasset"
    require(not snapshot.exists() and not backup.exists(), "存在先前备份，人工核对后再执行")
    profile_file = PACK / "Content/UI/Abilities/DA_DBA_Rooster_PlaceholderUIProfile.uasset"
    require(profile_file.is_file(), "现有Profile二进制文件缺失")
    snapshot.write_text(json.dumps(old, ensure_ascii=False, indent=2), encoding="utf-8")
    shutil.copy2(profile_file, backup)
    for row in missing:
        entry = unreal.DivineBeastsAbilityUIEntry()
        entry.set_editor_property("ability_id", unreal.Name(row["abilityId"]))
        entry.set_editor_property("display_name", unreal.Text(row["displayNameZh"]))
        entry.set_editor_property("description", unreal.Text(row["mechanismZh"] + "【V6美术预览，玩法待批准】"))
        entry.set_editor_property("icon", lib.load_asset(row["texturePath"]))
        entries.append(entry)
    profile.set_editor_property("entries", entries)
    profile.set_editor_property("art_preview_ability_ids",
                                [unreal.Name(r["abilityId"]) for r in plan["entries"]])
    require(lib.save_loaded_asset(profile, only_if_is_dirty=False), "Profile保存失败")
    # 二次只读快照：必须保留原有条目。
    latest = [snapshot_entry(e) for e in list(profile.get_editor_property("entries"))]
    require(all(x in latest for x in old), "旧Profile条目遭到修改")
    print("SSLM_ROOSTER_V6_PROFILE_NEW_ENTRIES", len(missing))

def verify(unreal, plan):
    lib = unreal.EditorAssetLibrary
    profile = inspect_single_profile(unreal, plan)
    entries = [snapshot_entry(e) for e in list(profile.get_editor_property("entries"))]
    by_id = {e["id"]: e for e in entries}
    require(len(by_id) == len(entries), "Profile包含重复AbilityId")
    for row in plan["entries"]:
        path = row["texturePath"]
        texture = lib.load_asset(path)
        require(isinstance(texture, unreal.Texture2D), "缺真实48纹理：" + path)
        require(texture.get_editor_property("srgb") is True, "sRGB未启用：" + path)
        require(texture.get_editor_property("compression_settings") == unreal.TextureCompressionSettings.TC_EDITOR_ICON, "UI压缩格式不符")
        entry = by_id.get(row["abilityId"])
        require(entry is not None and entry["icon"] == path, "Profile图标映射缺失：" + row["abilityId"])
    # ViewModel复用而非创建新体系；未授权时，技能栏必须为空。
    vm = unreal.new_object(unreal.DivineBeastsAbilityBarViewModel)
    require(len(vm.get_slots()) == 0 and len(vm.get_slot_details()) == 0, "未授权状态出现伪造技能")
    vm.unbind_from_loadout()
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    (LOG_DIR / "Verify.json").write_text(
        json.dumps({"heroId":plan["heroId"], "nativeUE48TextureCount":8,
                    "profileEntries":len(entries), "actualGameplayAuthorizationChanged":False,
                    "artPreviewOnly":True, "villageHUDVerified":False,
                    "arenaHUDVerified":False}, ensure_ascii=False, indent=2), encoding="utf-8")
    print("SSLM_ROOSTER_V6_NATIVE_PROFILE_VERIFY_OK")

def main():
    require(MODE in ("inspect", "import", "bind", "verify"), "未知执行模式")
    plan, zip_path, images = load_inputs()
    if zip_path is None:
        require(MODE == "inspect", "缺图片压缩包，不允许导入或绑定")
        return
    if MODE == "inspect":
        return
    unreal = check_editor()
    if MODE == "import":
        store_source(plan, images)
        import_textures(unreal, plan)
    elif MODE == "bind":
        bind_profile(unreal, plan)
    elif MODE == "verify":
        verify(unreal, plan)

main()
