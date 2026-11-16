#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 苍龙V6八技能已选图标导入UE5.8（仅创建新资源，不改服务器技能授权）。

接收用户在ChatGPT中确认的1A/2A/3C/4A/5A/6A/7A/8C所对应的8张独立AI原稿，
并通过用户认可的1254源图高质量缩小所得真实48x48 PNG，在已有
/DBAHeroPack_Dragon/UI/Abilities（辰龙唯一英雄内容包）创建8张Texture2D。
禁止将程序化Review候选冒充AI终稿、禁止重命名旧V1纹理或新增第二个PrimaryAsset。

执行前用户须将8张源图片（随对话提供的ZIP包）复制到同英雄包：
SourceArt/UI/SkillIcons/V6/Masters/T_DBA_Dragon_Sxx_*_V6_AI_Master_1254.png
SourceArt/UI/SkillIcons/V6/Final/48/T_DBA_Dragon_Sxx_*_V6_48.png

模式：
- inspect（默认）：Python只读检查源图指纹及正式UE目录；缺少源时报告待传入
- api（UE编辑器内）：只读检查UE5.8纹理枚举和导入接口，不写资产
- apply（UE编辑器内）：检查八份源完整、5份旧V1保留、8个目标不存在，然后真ImportAssetTask保存
- verify（**第二个**独立UE编辑器进程）：核对8份Texture2D原生尺寸、sRGB、UI压缩、纹理组、无Mip、来源
只允许新建八张纹理，不修改原占位UIProfile/AbilitySet；后续正式绑定由用户另行验收。
"""

from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MODE=os.environ.get("SSLM_DRAGON_V6_ICON_MODE","inspect").strip().lower()
PACK=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon")
SRC_ROOT=PACK/"SourceArt/UI/SkillIcons/V6"
UE_DIR="/DBAHeroPack_Dragon/UI/Abilities"
UE_ASSET_ROOT=PACK/"Content/UI/Abilities"
RECEIPT=ROOT/"Saved/ArtReview/DragonV6Selected8NativeIcons_20261018/UEImportedTextures.json"
NAMES=("CangLongZhua","LeiCai","LeiXi","CangLongDingTian",
       "YuFeng","YunHu","LongYa","LongYu")
LABELS=("苍龙爪","雷裁","雷息","苍龙定天","御风","云护","龙压","龙域")
V5_LABELS=("苍龙爪","雷裁","雷息","三象域","御风","云盾","风压环","风云域")
SLOTS=("Primary","Slot1","Slot2","Slot4","Slot3","Slot5","Slot6","Slot7")
ABILITIES=(
    "dba.ability.dragon_eight_dragon_claw@1",
    "dba.ability.dragon_eight_cloud_scale@1",
    "dba.ability.dragon_eight_azure_breath@1",
    "dba.ability.dragon_eight_dragon_ascension@1",
    "dba.ability.dragon_eight_cloud_stride@1",
    "dba.ability.dragon_eight_dragon_scale_guard@1",
    "dba.ability.dragon_eight_thunder_cloud_lock@1",
    "dba.ability.dragon_eight_cloud_blessing@1",
)
VARIANTS=("A","A","C","A","A","A","A","C")
SOURCE_HASHES=(
    "a42677170fe4f0a7930798c504035d32f86afb2819e115584be5648284457d0a",
    "759770cefa72e7fd8cef9a47dd5b17846bda611e9fc48ae26f749f58d187c9d4",
    "396f29535b230c091c697bbc83d91da4ac2bab0464ddd29eb08adbfa8fa61676",
    "50c658df62b8c437c7798dceb5aaa20ee4b4892dfe189080df18acff793ed688",
    "5bec74e953cf2ec16c79cb65576734a541724f49998427dad61210706014433d",
    "e488b12eba0e612c4cc21db4670269d0340fc87b13de7a1dba33bc09493d0a1f",
    "83024c10e261c16722caed861542a39b9ef8d18d60e155138aae55916917a465",
    "0ae8e119916b58410ad05adf8bbc049211cf1a1e34846078d782fa65b36afc24",
)
RUNTIME_HASHES=(
    "f88e6d7bead0bf846a99faad77d4ed61c6661d865e573b219202dc8201c179f0",
    "22efe03f1a1e40f25c2111034de5c0881e148393a9a5d9480801ac78e6e2b4a8",
    "53ab05d9992692f9ec9a87200eba0f6424715e254a991fa75275bfa85cc4b6d2",
    "92d7c0fd584cd412d80d479b7cfdc093321ba610479e8d6fd344acb87a5fd6a4",
    "edfdcd0e05a22326ed64098d2f063ce5ae5524d836f048c75480e08f066d85a4",
    "f5fe6aa3ae894a53e7740ce2ce4e11c575e5516e9b3615ecc6b4a71fd3fd2ee5",
    "79ebe59a078d90df357501e4399c214a27146063561bed355483b17836733845",
    "986fd085a25d6576582938557da65cb5f5dec12f40f41499369fefd9f52459ac",
)
OLD_V1=(
    "T_DBA_Dragon_BasicAttack_DragonClaw_V1.uasset",
    "T_DBA_Dragon_Active01_DragonSpiral_V1.uasset",
    "T_DBA_Dragon_Active02_CloudBeam_V1.uasset",
    "T_DBA_Dragon_Ultimate_DragonAscension_V1.uasset",
    "T_DBA_Dragon_Passive_CloudHeart_V1.uasset",
)
PROFILE=UE_ASSET_ROOT/"DA_DBA_Dragon_PlaceholderUIProfile.uasset"


def sha(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda:file.read(1024*1024),b""):h.update(block)
    return h.hexdigest()


def entries():
    out=[]
    for k in range(8):
        name=f"T_DBA_Dragon_S{k+1:02d}_{NAMES[k]}_V6"
        out.append({
            "index":k+1,"skill":LABELS[k],"productionNameV5":V5_LABELS[k],
            "abilityId":ABILITIES[k],"slot":SLOTS[k],"variant":VARIANTS[k],
            "name":name,
            "master":SRC_ROOT/"Masters"/(name+"_AI_Master_1254.png"),
            "source":SRC_ROOT/"Final/48"/(name+"_48.png"),
            "masterSha":SOURCE_HASHES[k],"pngSha":RUNTIME_HASHES[k],
            "uePath":f"{UE_DIR}/{name}",
            "diskPath":UE_ASSET_ROOT/(name+".uasset"),
        })
    return out


def png_size(p:Path):
    with p.open("rb") as f:
        header=f.read(24)
    if len(header)<24 or header[:8]!=b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("不是可解码的PNG或文件头不完整："+str(p))
    import struct
    return struct.unpack(">II",header[16:24])


def preflight(require_sources:bool):
    if MODE not in ("inspect","api","apply","verify"):
        raise ValueError("合法模式仅 inspect/api/apply/verify")
    if not PROFILE.is_file() or not UE_ASSET_ROOT.is_dir():
        raise RuntimeError("辰龙现有主资产或英雄内容包不存在；禁止创建平行包")
    missing=[]
    for row in entries():
        for kind,key,dim in (("master","masterSha",1254),("source","pngSha",48)):
            p=row[kind]
            if not p.is_file():
                missing.append(str(p.relative_to(ROOT)))
                continue
            if sha(p)!=row[key] or png_size(p)!=(dim,dim):
                raise RuntimeError(f"{row['index']}号技能{kind}源图SHA或实际尺寸与已选独立AI图不匹配")
    for file in OLD_V1:
        if not (UE_ASSET_ROOT/file).is_file():
            raise RuntimeError("旧V1资产缺失，拒绝覆盖新版本："+file)
    if require_sources and missing:
        raise RuntimeError(f"缺少{len(missing)}份高质量主图或48px图片，不能使用程序化草稿代替："
                           +";".join(missing[:5]))
    print("SSLM_DRAGON_V6_SOURCES_PREFLIGHT",
          json.dumps({"expected":16,"missingCount":len(missing),
                      "missingExamples":missing[:5],"oldV1Preserved":len(OLD_V1),
                      "uiProfileUntouched":True},ensure_ascii=False),flush=True)
    return not missing


def unreal_editor():
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不是正式SSLM UE5.8工程，拒绝创建Dragon Texture2D")
    return unreal


def native_enums(unreal):
    vals=(
       (unreal.TextureCompressionSettings,"TC_EDITOR_ICON"),
       (unreal.TextureGroup,"TEXTUREGROUP_UI"),
       (unreal.TextureMipGenSettings,"TMGS_NO_MIPMAPS"),
       (unreal.TextureFilter,"TF_BILINEAR"),
       (unreal.TextureAddress,"TA_CLAMP"),
    )
    missing=[name for owner,name in vals if not hasattr(owner,name)]
    if missing:
        raise RuntimeError("当前UE5.8无法确定对应UI贴图压缩/纹理组/过滤设置："+",".join(missing))
    print("SSLM_DRAGON_V6_UE_NATIVE_UI_ENUMS_VALIDATED",len(vals),flush=True)
    return tuple(getattr(owner,name) for owner,name in vals)


def check_texture(unreal,row,require_origin=True):
    lib=unreal.EditorAssetLibrary
    tex=lib.load_asset(row["uePath"])
    if not isinstance(tex,unreal.Texture2D):
        raise RuntimeError("未找到真UE Texture2D："+row["uePath"])
    compression,group,mip,flt,addr=native_enums(unreal)
    expected=(
        ("srgb",True),
        ("compression_settings",compression),
        ("lod_group",group),
        ("mip_gen_settings",mip),
        ("filter",flt),
        ("address_x",addr),
        ("address_y",addr),
        ("never_stream",True),
    )
    for prop,value in expected:
        if tex.get_editor_property(prop)!=value:
            raise RuntimeError(f"{row['name']}原生纹理属性{prop}不等于UI规范")
    data=lib.find_asset_data(row["uePath"])
    if not data.is_valid() or str(data.get_tag_value("Dimensions") or "")!="48x48":
        raise RuntimeError("UE图标Texture2D不是实际48×48："+row["name"])
    if require_origin:
        origin=tex.get_editor_property("asset_import_data")
        path=str(origin.get_first_filename()).replace("\\","/").lower() if origin else ""
        if path!=str(row["source"]).replace("\\","/").lower():
            raise RuntimeError("导入源路径与已选PNG不一致："+row["name"])
    return tex


def apply(unreal):
    if RECEIPT.exists():
        raise RuntimeError("原生导入收据已存在，不能重复写入，改用verify")
    lib=unreal.EditorAssetLibrary
    if not lib.does_directory_exist(UE_DIR):
        raise RuntimeError("已有辰龙英雄内容包UI/Abilities路径未在UE挂载")
    for row in entries():
        if lib.does_asset_exist(row["uePath"]) or row["diskPath"].exists():
            raise RuntimeError("本技能已有Texture2D或磁盘资产；禁止覆盖："+row["uePath"])
    old_checks={p:sha(UE_ASSET_ROOT/p) for p in OLD_V1}
    profile_sha=sha(PROFILE)
    compression,group,mip,flt,addr=native_enums(unreal)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    imported=[]
    for row in entries():
        task=unreal.AssetImportTask()
        task.set_editor_property("filename",str(row["source"]))
        task.set_editor_property("destination_path",UE_DIR)
        task.set_editor_property("destination_name",row["name"])
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("automated",True)
        task.set_editor_property("save",False)
        tools.import_asset_tasks([task])
        paths=task.get_editor_property("imported_object_paths")
        if not paths or not lib.does_asset_exist(row["uePath"]):
            raise RuntimeError("UE原生导入没有创建指定Texture2D，不能宣称完成："+row["name"])
        tex=lib.load_asset(row["uePath"])
        if not isinstance(tex,unreal.Texture2D):
            raise RuntimeError("UE资产导入结果不是真正Texture2D："+row["name"])
        for prop,val in (
            ("srgb",True),("compression_settings",compression),("lod_group",group),
            ("mip_gen_settings",mip),("filter",flt),("address_x",addr),("address_y",addr),
            ("never_stream",True),
        ):tex.set_editor_property(prop,val)
        if not lib.save_loaded_asset(tex,only_if_is_dirty=False):
            raise RuntimeError("UE原生纹理保存失败："+row["name"])
        check_texture(unreal,row)
        imported.append(row["name"])
        print("SSLM_DRAGON_V6_NATIVE_TEXTURE_IMPORTED",row["index"],row["name"],flush=True)
    if any(sha(UE_ASSET_ROOT/p)!=h for p,h in old_checks.items()) or sha(PROFILE)!=profile_sha:
        raise RuntimeError("旧图标或唯一生产UIProfile被意外修改，请立即核查")
    RECEIPT.parent.mkdir(parents=True,exist_ok=True)
    RECEIPT.write_text(json.dumps({
        "schemaVersion":1,"heroId":"Hero.Zodiac.Dragon",
        "date":"2026-10-18",
        "importedCount":len(imported),"items":imported,
        "productionV5NameAuthority":True,
        "profileUnchangedSha256":profile_sha,
        "oldV1Sha256":old_checks,
        "coldVerified":False,"profileBound":False,
        "combatHUDValidated":False,
        "humanArtisticallyApproved":False,
    },ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("SSLM_DRAGON_V6_NATIVE_UE_EIGHT_TEXTURES_SAVED count=8 cold_verify_pending=true",flush=True)


def verify(unreal):
    if not RECEIPT.is_file():
        raise RuntimeError("缺少原生真实UE导入收据，禁止虚构冷重载通过")
    record=json.loads(RECEIPT.read_text(encoding="utf-8"))
    if record.get("importedCount")!=8 or len(record.get("items",[]))!=8:
        raise RuntimeError("原生8图导入收据不完整")
    for row in entries():
        check_texture(unreal,row)
    if sha(PROFILE)!=record["profileUnchangedSha256"]:
        raise RuntimeError("唯一现有Dragon UIProfile被其它任务改动")
    for name,orig in record["oldV1Sha256"].items():
        if sha(UE_ASSET_ROOT/name)!=orig:
            raise RuntimeError("原V1图标被修改，违反回退合同："+name)
    record["coldVerified"]=True
    RECEIPT.write_text(json.dumps(record,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("SSLM_DRAGON_V6_EIGHT_TEXTURES_IN_HERO_PACK_COLD_VERIFY_PASS",len(entries()),flush=True)


def main():
    ready=preflight(require_sources=MODE in ("apply","verify"))
    if MODE=="inspect":
        print("SSLM_DRAGON_V6_TEXTURE_IMPORT_READY" if ready
              else "SSLM_DRAGON_V6_SOURCE_TRANSFER_PENDING",flush=True)
        return
    unreal=unreal_editor()
    native_enums(unreal)
    if MODE=="api":return
    if MODE=="apply":apply(unreal)
    else:verify(unreal)


if __name__=="__main__":main()
