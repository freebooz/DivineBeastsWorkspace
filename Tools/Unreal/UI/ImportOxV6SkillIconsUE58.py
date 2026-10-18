# -*- coding: utf-8 -*-
"""SSLM丑牛V6八技能高质量图标：UE5.8原生纹理和唯一Ox UIProfile的安全作者。

默认 mode=inspect（只读）；mode=import（只导入图片，未授权玩法不变）；
mode=bind（人工签收后，原位增补现有Ox UIProfile八条V6展示条目）。
不新建重复HeroDefinitionId的PrimaryDataAsset；不修改GAS AbilitySet或默认技能授予。
图像由会话用户提供的合法附件ZIP安全导入工作区Game/Saved/ArtReview后再执行。
"""
from __future__ import annotations
import hashlib
import json
import os
import struct
import zipfile
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
MANIFEST = WORKSPACE / "Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json"
DESIGN = WORKSPACE / "Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json"
REGISTRY = WORKSPACE / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json"
ROOT = "/DBAHeroPack_Ox/UI/Abilities"
PROFILE = ROOT + "/DA_DBA_Ox_PlaceholderUIProfile"
SOURCE_ART = WORKSPACE / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/SourceArt/UI/SkillIcons/V6"
MODE = os.environ.get("SSLM_OX_V6_ICON_MODE", "inspect").strip().lower()
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

def require(condition, message):
    if not condition:
        raise RuntimeError("SSLM丑牛V6图标门禁：" + message)

def source_contract():
    model = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    design = json.loads(DESIGN.read_text(encoding="utf-8-sig"))
    registry = json.loads(REGISTRY.read_text(encoding="utf-8-sig"))
    hero = next((x for x in design["heroes"] if x["heroId"] == "Hero.Zodiac.Ox"), None)
    drafts = sorted(hero["skills"], key=lambda x: x["displayOrder"]) if hero else []
    rows = model.get("items", [])
    reg = {x["abilityId"]:x for x in registry["skills"] if x["heroId"]=="Hero.Zodiac.Ox"}
    require(model["heroId"]=="Hero.Zodiac.Ox" and model["schemaVersion"]==1
            and len(rows)==8 and len(drafts)==8, "不完整的八技能原始设计或清单")
    require(model["duplicatePrimaryAssetProfileForbidden"],"不得创建第二个Ox UIProfile")
    for i,(row,draft) in enumerate(zip(rows,drafts)):
        require(row["order"]==i+1 and row["abilityId"]==draft["abilityId"]
                and row["slotId"]==draft["slotId"] and row["displayNameZh"]==draft["proposedNameZh"]
                and row["runtimeModeCandidate"]==reg[row["abilityId"]]["proposedPrimitiveCandidate"],
                "V6技能身份、名称、原语及槽位必须完全符合项目事实："+str(i+1))
        require(row["targetTexturePath"].startswith(ROOT+"/V6/Textures/T_DBA_Ox_V6_"),
                "UI纹理挂载点不允许跨包")
        require(len(row["candidateFiles"])==3 and row["qualityGate"]=="HUMAN_APPROVAL_REQUIRED",
                "候选未齐或缺人工审核门禁")
    return model, rows

def image_info(blob):
    if blob[:8]!=PNG_SIGNATURE or len(blob)<28: raise ValueError("不是完整PNG")
    return struct.unpack(">II",blob[16:24])

def inspect_zip(model,rows):
    """只校验字节，永不随意解压不信任的ZIP路径。"""
    arc = WORKSPACE / model["pendingImageArchive"]
    if not arc.is_file():
        print("SSLM_OX_V6_IMPORT_WAITING_FOR_TRUSTED_ART_UPLOAD",arc)
        return None
    require(arc.stat().st_size<120*1024*1024,"音源ZIP过大，拒绝导入")
    with zipfile.ZipFile(arc) as z:
        members={name for name in z.namelist() if not name.endswith("/")}
        require("OxV6_IconManifest.json" in members,"缺原图来源及SHA审计清单")
        source = json.loads(z.read("OxV6_IconManifest.json").decode("utf-8-sig"))
        supplied=source.get("entries",[])
        require(source.get("heroId")=="Hero.Zodiac.Ox" and len(supplied)==8,
                "ZIP不包含牛8技能的签发身份")
        for row,record in zip(rows,supplied):
            require(row["abilityId"]==record["abilityId"] and row["artFileName"]==record["assetName"],
                    "图片包技能身份或文件名被篡改")
            for key,expect in ((row["preview512"],(512,512)),(row["preview256"],(256,256)),
                               (row["preview64"],(64,64)),(row["preview48"],(48,48))):
                require(key in members and not key.startswith("/") and ".." not in Path(key).parts,
                        "图片文件路径不安全或缺失："+key)
                b=z.read(key)
                require(image_info(b)==expect,"图像大小与声称分辨率不符："+key)
            require(hashlib.sha256(z.read(row["preview256"])).hexdigest()==record["ui256Sha256"],
                    "256px源图SHA-256不匹配："+row["artFileName"])
            require(hashlib.sha256(z.read(row["preview48"])).hexdigest()==record["ui48Sha256"],
                    "48px源图SHA-256不匹配："+row["artFileName"])
        print("SSLM_OX_V6_ZIP_PREFLIGHT_OK",len(rows),"candidates_each",3)
    return arc

def import_assets(model,rows,archive):
    import unreal
    command=unreal.SystemLibrary.get_command_line()
    require("-SSLMOxV6IconAuthoring" in command,
            "实际生成uasset需要显式编辑器参数 -SSLMOxV6IconAuthoring")
    require(Path(unreal.Paths.get_project_file_path()).resolve()==
            (WORKSPACE/"Game/DivineBeastsArena.uproject").resolve(), "非正式SSLM工程")
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/DBAHeroPack_Ox"],True)
    library=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    with zipfile.ZipFile(archive) as z:
        for row in rows:
            path=row["targetTexturePath"]
            directory,name=path.rsplit("/",1)
            src=SOURCE_ART/(name+".png")  # 严格48x48运行源图
            raw=z.read(row["preview48"])
            master=SOURCE_ART/(name+"_Source256.png") # 保留256x256高精备份
            master_blob=z.read(row["preview256"])
            src.parent.mkdir(parents=True,exist_ok=True)
            for filename,blob in ((src,raw),(master,master_blob)):
                if filename.exists():
                    require(hashlib.sha256(filename.read_bytes()).digest()==hashlib.sha256(blob).digest(),
                            "已有源图与会话作者图不一致，拒绝覆盖："+str(filename))
                else:
                    filename.write_bytes(blob)
            if library.does_asset_exist(path):
                asset=library.load_asset(path)
                require(isinstance(asset,unreal.Texture2D),
                        "原生资产路径已有不同类型，不覆盖："+path)
                print("SSLM_OX_V6_TEXTURE_EXISTS",name)
            else:
                task=unreal.AssetImportTask()
                task.set_editor_property("filename",str(src))
                task.set_editor_property("destination_name",name)
                task.set_editor_property("destination_path",directory)
                task.set_editor_property("replace_existing",False)
                task.set_editor_property("automated",True)
                task.set_editor_property("save",False)
                tools.import_asset_tasks([task])
                asset=library.load_asset(path)
                require(isinstance(asset,unreal.Texture2D),"导入没有生成真实Texture2D："+path)
                print("SSLM_OX_V6_TEXTURE_IMPORTED",name)
            # 正式Texture2D实际48×48像素；256×256原图保留在SourceArt以供改版。
            asset.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            asset.set_editor_property("lod_group",unreal.TextureGroup.TEXTUREGROUP_UI)
            asset.set_editor_property("mip_gen_settings",unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            asset.set_editor_property("srgb",True)
            asset.set_editor_property("never_stream",True)
            asset.set_editor_property("filter",unreal.TextureFilter.TF_BILINEAR)
            require(library.save_loaded_asset(asset,only_if_is_dirty=False),"纹理没有真实保存："+path)
            reloaded=library.load_asset(path)
            require(isinstance(reloaded,unreal.Texture2D),"纹理保存后重新读取失败："+path)
        print("SSLM_OX_V6_TEXTURES_IMPORTED_AND_SAVED",len(rows))
    return True

def bind_existing_profile(model,rows):
    import unreal
    require(model["manualReviewStatus"]=="APPROVED","人工美术审核未批准，不绑定正式技能栏")
    require("-SSLMOxV6UIProfileBind" in unreal.SystemLibrary.get_command_line(),
            "增补现有Profile必须显式 -SSLMOxV6UIProfileBind")
    library=unreal.EditorAssetLibrary
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/DBAHeroPack_Ox"],True)
    profile=library.load_asset(PROFILE)
    require(isinstance(profile,unreal.DivineBeastsAbilityUIProfile),
            "唯一Ox UIProfile丢失或类型错误")
    require(str(profile.get_editor_property("hero_definition_id"))=="Hero.Zodiac.Ox",
            "旧UI Profile并非Ox，禁止覆写")
    old=list(profile.get_editor_property("entries"))
    known={str(e.get_editor_property("ability_id")) for e in old}
    # 当前开发占位授权不得被V6候选误改；按稳定AbilityId原位增补未授权的纯UI元数据。
    placeholder={x for x in known if x.startswith("dba.ability.ox_placeholder_")}
    require(len(placeholder)>=6 and all(row["abilityId"] not in known for row in rows),
            "原占位条目不足或V6条目已经存在；必须先人工对照合并")
    require(len(old)+8<=64,"UIProfile条目数量超出平台上限")
    for row in rows:
        tex=library.load_asset(row["targetTexturePath"])
        require(isinstance(tex,unreal.Texture2D),"V6技能纹理不存在："+row["targetTexturePath"])
        entry=unreal.DivineBeastsAbilityUIEntry()
        entry.set_editor_property("ability_id",unreal.Name(row["abilityId"]))
        entry.set_editor_property("display_name",unreal.Text(row["displayNameZh"]))
        entry.set_editor_property("description",unreal.Text(row["mechanismZh"]))
        entry.set_editor_property("icon",tex)
        old.append(entry)
    profile.set_editor_property("entries",old)
    require(library.save_loaded_asset(profile,only_if_is_dirty=False),"真实Ox UIProfile无法保存")
    require(len(library.load_asset(PROFILE).get_editor_property("entries"))>=14,
            "Ox V6 UIProfile原生回读失败")
    print("SSLM_OX_V6_UI_PROFILE_EXTENDED",len(rows),"GRANTS_UNCHANGED",True)

def run():
    model,rows=source_contract()
    print("SSLM_OX_V6_IDENTITY_PREFLIGHT_OK",len(rows),MODE)
    archive=inspect_zip(model,rows)
    if MODE=="inspect":return
    require(archive is not None,"还没有可验证的会话原画ZIP，不能假称完成纹理导入")
    require(MODE in ("import","bind"),"模式仅允许inspect/import/bind")
    import_assets(model,rows,archive)
    if MODE=="bind":bind_existing_profile(model,rows)

if __name__=="__main__":
    run()
