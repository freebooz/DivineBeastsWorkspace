# -*- coding: utf-8 -*-
"""SSLM 戌狗 V6 图标：真实UE5.8纹理导入、现有Profile预览顺序及原生证据。

适用范围：通过Monolith的 editor.run_python 执行，只有已人工检查来源的48/64 PNG
进入本工作空间后才可执行导入。默认inspect不改变资产。V6美术显示不代表服务器
GAS授权或V6改名正式投产。已有五张V1纹理和唯一Profile必须保持原样。
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog"
FREEZE = PACK / "Docs/DogV6_ArtFreeze_20261019.json"
STAGING = PACK / "Docs/DogV6_StagingManifest.json"
PLAN = ROOT / "Saved/Validation/DogV6Staging20261019/IntegrationPlan.json"
OUT = ROOT / "Saved/Validation/DogV6Staging20261019/UEImportEvidence"
MODE = str(globals().get("DOG_V6_MODE", os.getenv("SSLM_DOG_V6_MODE", "inspect"))).strip().lower()
EXPECTED_SLOTS = ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"]
PROFILE_PATH = "/DBAHeroPack_Dog/UI/Abilities/DA_DBA_Dog_PlaceholderUIProfile"


def check(ok, message):
    """统一失败即停止；不把缺失文件解释成已完成资源。"""
    if not ok:
        raise RuntimeError("戌狗V6资源门禁：" + str(message))


def digest(path):
    """对真实文件计算SHA-256，防止复用旧图时发生悄然漂移。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dimensions(path):
    """直接从PNG标准IHDR读取宽高，无需依赖外部Pillow。"""
    data = path.read_bytes()[:24]
    check(len(data) == 24 and data[:8] == b"\x89PNG\r\n\x1a\n", "缺少有效PNG：" + str(path))
    return struct.unpack(">II", data[16:24])


def project_file(relative):
    """将清单相对路径严格约束在本工作空间内。"""
    path = (ROOT / relative).resolve()
    check(path.is_relative_to(ROOT.resolve()), "清单中的文件路径越出工作空间")
    return path


def read_input(strict):
    """冻结身份和实际暂存源双向核对；正式名称不得从V6临时代替V5。"""
    freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
    check(freeze["heroId"] == "Hero.Zodiac.Dog" and len(freeze["skills"]) == 8, "冻结技能身份错误")
    check([x["slotId"] for x in freeze["skills"]] == EXPECTED_SLOTS, "技能槽位错配")
    missing = [str(f) for f in (STAGING, PLAN) if not f.is_file()]
    if missing:
        if strict:
            check(False, "源纹理尚未传入Windows工程：" + ",".join(missing))
        return freeze, None, None, missing
    stage = json.loads(STAGING.read_text(encoding="utf-8"))
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    check(stage["heroId"] == "Hero.Zodiac.Dog" and len(stage["skills"]) == 8, "图源清单英雄身份错误")
    check(len(plan.get("heroes", [])) == 1 and plan["heroes"][0]["hero"] == "Dog", "唯一Profile映射合同缺失")
    mappings = plan["heroes"][0]["entries"]
    check(len(mappings) == 8 and [x["slotId"] for x in mappings] == EXPECTED_SLOTS, "原始技能槽位映射错误")
    missing = []
    for spec, art, row in zip(freeze["skills"], stage["skills"], mappings):
        check(spec["abilityId"] == art["abilityId"] == row["abilityId"], "稳定AbilityId与图源不一致")
        check(spec["productionV5NameZh"] == row["displayNameZh"], "未经批准修改V5正式显示名称")
        check(spec["texturePath"] == row["texturePath"], "图标路径与冻结身份不一致")
        for key, size, index in [("runtime48",48,"png48"), ("audit64",64,"png64")]:
            record = art["files"][key]
            path = project_file(record["relativePath"])
            if not path.exists():
                missing.append(str(path))
                continue
            check(dimensions(path) == (size,size), "PNG真实尺寸错误：" + str(path))
            check(digest(path) == record["sha256"] == row["sha"+str(size)], "PNG的SHA-256与合同不一致：" + str(path))
    if strict:
        check(not missing, "真实PNG缺失：" + ";".join(missing))
    return freeze, stage, plan, missing


def engine():
    """真正原生资源写入必须由已加载正式项目的UE5.8编辑器执行。"""
    import unreal
    check(Path(unreal.Paths.get_project_file_path()).resolve() ==
          (ROOT/"Game/DivineBeastsArena.uproject").resolve(), "不是SSLM正式工程")
    check(unreal.SystemLibrary.get_engine_version().startswith("5.8."), "不是锁定UE5.8")
    # PythonScriptCommandlet（命令行Python执行器）可能先于异步资产目录扫描完成。
    # 内容包已由uproject启用并挂载，此处只对目标英雄包强制同步扫描，
    # 不扫描全工程，也不创建缺失Profile掩盖实际错误。
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/DBAHeroPack_Dog/UI/Abilities"],force_rescan=True)
    return unreal


def verify_textures(unreal, freeze):
    """核查八张已保存的原生Texture2D而不是只检查磁盘PNG。"""
    lib = unreal.EditorAssetLibrary
    report = []
    for spec in freeze["skills"]:
        pkg = spec["texturePath"]
        texture = lib.load_asset(pkg)
        check(isinstance(texture, unreal.Texture2D), "原生纹理不存在："+pkg)
        asset_data = lib.find_asset_data(pkg)
        shape = str(asset_data.get_tag_value("Dimensions")).lower()
        check(shape == "48x48", "原生纹理尺寸不是48x48：" + pkg + " " + shape)
        check(texture.get_editor_property("srgb"), "未开启sRGB：" + pkg)
        check(texture.get_editor_property("compression_settings") ==
              unreal.TextureCompressionSettings.TC_EDITOR_ICON, "压缩模式错误：" + pkg)
        check(texture.get_editor_property("mip_gen_settings") ==
              unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS, "多级纹理配置错误：" + pkg)
        report.append({"asset":pkg, "dimensions":shape, "class":"Texture2D"})
    return report


def import_preview(unreal, freeze, stage):
    """幂等的增量导入：目标重名但无本次可信凭据时直接失败。"""
    check(MODE == "import_preview", "导入模式错误")
    lib = unreal.EditorAssetLibrary
    for spec in freeze["skills"]:
        check(not lib.does_asset_exist(spec["texturePath"]),
              "目标V6纹理已存在，禁止在缺乏前次SHA证明时覆盖：" + spec["texturePath"])
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for spec, art in zip(freeze["skills"], stage["skills"]):
        package_name = spec["texturePath"]
        folder, name = package_name.rsplit("/", 1)
        source = project_file(art["files"]["runtime48"]["relativePath"])
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        texture = lib.load_asset(package_name)
        check(isinstance(texture, unreal.Texture2D), "编辑器未创建真正Texture2D：" + package_name)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
        check(lib.save_loaded_asset(texture, only_if_is_dirty=False), "纹理保存失败：" + package_name)
    return verify_textures(unreal, freeze)


def snapshots(unreal, profile):
    """快照只包含原有显示内容，不能移除或修改旧条目。"""
    snapshot = []
    for entry in list(profile.get_editor_property("entries")):
        icon = entry.get_editor_property("icon")
        icon_path = icon.get_path_name().split(".")[0] if icon else ""
        snapshot.append({"id":str(entry.get_editor_property("ability_id")),
                         "name":str(entry.get_editor_property("display_name")),
                         "description":str(entry.get_editor_property("description")),
                         "icon":icon_path})
    return snapshot


def add_preview_order(unreal, freeze, plan):
    """已有通用Profile工具负责增补条目；此处只追加八槽只读顺序。"""
    lib = unreal.EditorAssetLibrary
    profile = lib.load_asset(PROFILE_PATH)
    check(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), "现有唯一Profile不可加载")
    check(str(profile.get_editor_property("hero_definition_id")) == "Hero.Zodiac.Dog", "Profile英雄身份错误")
    entries = snapshots(unreal, profile)
    byid = {r["id"]:r for r in entries}
    check(len(entries) == len(byid), "旧Profile中有重复技能ID")
    for spec, row in zip(freeze["skills"], plan["heroes"][0]["entries"]):
        x = byid.get(spec["abilityId"])
        check(x is not None and x["name"] == row["displayNameZh"] and
              x["icon"] == spec["texturePath"] and x["description"].strip(),
              "需要先通过通用Profile工具增补八项真实展示条目：" + spec["abilityId"])
    order = [str(n) for n in profile.get_editor_property("art_preview_ability_ids")]
    target = [s["abilityId"] for s in freeze["skills"]]
    check(not order or order == target, "已有不一致的八槽预览身份，不能覆盖")
    if order == target:
        return {"changed":False,"entries":len(entries),"artPreviewAbilityIds":target}
    OUT.mkdir(parents=True, exist_ok=True)
    backup = OUT / "Dog_Profile_BeforePreviewOrder.uasset"
    snapshot = OUT / "Dog_Profile_EntriesBeforePreviewOrder.json"
    check(not backup.exists() and not snapshot.exists(), "备份已存在，需人工确认以防覆盖旧恢复点")
    disk = PACK / "Content/UI/Abilities/DA_DBA_Dog_PlaceholderUIProfile.uasset"
    check(disk.is_file(), "既有Profile文件不存在")
    snapshot.write_text(json.dumps(entries,ensure_ascii=False,indent=2),encoding="utf-8")
    shutil.copy2(disk, backup)
    profile.set_editor_property("art_preview_ability_ids",[unreal.Name(i) for i in target])
    check(lib.save_loaded_asset(profile, only_if_is_dirty=False), "原生Profile保存失败")
    check(all(x in snapshots(unreal,profile) for x in entries), "修改预览顺序竟改变了原有展示条目")
    return {"changed":True,"entries":len(entries),"artPreviewAbilityIds":target}


def main():
    """只读模式可报告缺失；修改模式必须先完成全部输入预检。"""
    check(MODE in ("inspect","probe_ue","import_preview","verify","preview_order"), "模式未受支持")
    freeze, stage, plan, missing = read_input(MODE != "inspect")
    if MODE == "inspect":
        print(json.dumps({"status":"AWAITING_ART_TRANSFER" if missing else "SOURCES_VALID",
                          "heroId":"Hero.Zodiac.Dog","filesMissing":missing,
                          "nativeTexture2DVerified":False,"hudVerified":False},
                         ensure_ascii=False))
        return
    unreal = engine()
    if MODE == "probe_ue":
        lib = unreal.EditorAssetLibrary
        profile = lib.load_asset(PROFILE_PATH)
        check(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), "既有唯一戌狗Profile类型未加载")
        check(str(profile.get_editor_property("hero_definition_id")) == "Hero.Zodiac.Dog", "戌狗Profile身份错误")
        check(unreal.AssetToolsHelpers.get_asset_tools() is not None, "引擎资产导入接口不可用")
        check(unreal.TextureCompressionSettings.TC_EDITOR_ICON is not None, "未发现项目已有UI压缩模式")
        check(unreal.TextureGroup.TEXTUREGROUP_UI is not None and
              unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS is not None and
              unreal.TextureFilter.TF_BILINEAR is not None, "引擎UI纹理属性不兼容")
        check(not any(lib.does_asset_exist(x["texturePath"]) for x in freeze["skills"]),
              "检测到已存在的V6原生纹理，需先核查幂等关系")
        print(json.dumps({"status":"UE_NATIVE_CAPABILITIES_READY",
                          "heroId":"Hero.Zodiac.Dog","engineVersion":unreal.SystemLibrary.get_engine_version(),
                          "profile":"existing_unique","source48":8,"source64":8,
                          "newTextures":0,"next":"import_preview"},ensure_ascii=False))
        return
    if MODE == "import_preview":
        result = {"textures":import_preview(unreal, freeze, stage)}
    elif MODE == "preview_order":
        verify_textures(unreal, freeze)
        result = add_preview_order(unreal, freeze, plan)
    else:
        result = {"textures":verify_textures(unreal, freeze)}
    OUT.mkdir(parents=True, exist_ok=True)
    result["mode"] = MODE
    result["heroId"] = "Hero.Zodiac.Dog"
    result["abilityGranted"] = False
    result["combatHUDVerified"] = False
    (OUT/("DogV6_"+MODE+".json")).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps(result,ensure_ascii=False))


main()
