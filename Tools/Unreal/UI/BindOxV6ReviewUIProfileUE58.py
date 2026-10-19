# -*- coding: utf-8 -*-
"""SSLM 丑牛 V6 八技能图标接入唯一 UI Profile（纯客户端表现元数据）。

默认 inspect（只读检查）。apply（明确授权）只向现有牛英雄UIProfile增补8条V6技能显示数据；
不会创建第二个相同HeroDefinitionId主资产，不修改GAS技能集或任何服务器授予，
更不会更改美术审批PENDING状态。重复调用：已有正确映射时安全跳过；不覆盖冲突条目。
必须由UE5.8正式编辑器进程运行，并带 -SSLMOxV6ReviewOnlyProfileAuthoring 命令行门禁。
"""
from __future__ import annotations
import json
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
MANIFEST = WORKSPACE / "Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json"
DESIGN = WORKSPACE / "Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json"
REGISTRY = WORKSPACE / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json"
PROFILE_PATH = "/DBAHeroPack_Ox/UI/Abilities/DA_DBA_Ox_PlaceholderUIProfile"
PROFILE_FILE = WORKSPACE / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Ox/Content/UI/Abilities/DA_DBA_Ox_PlaceholderUIProfile.uasset"
MODE = os.environ.get("SSLM_OX_V6_PROFILE_MODE", "inspect").strip().lower()


def require(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError("SSLM丑牛V6技能UI配置门禁：" + message)


def read_source():
    m = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    design = json.loads(DESIGN.read_text(encoding="utf-8-sig"))
    register = json.loads(REGISTRY.read_text(encoding="utf-8-sig"))
    hero = next(x for x in design["heroes"] if x["heroId"] == "Hero.Zodiac.Ox")
    skills = sorted(hero["skills"], key=lambda x: x["displayOrder"])
    rows = m.get("items", [])
    reg = {x["abilityId"]: x for x in register["skills"] if x["heroId"] == "Hero.Zodiac.Ox"}
    require(m["heroId"] == "Hero.Zodiac.Ox" and m["duplicatePrimaryAssetProfileForbidden"]
            and len(rows) == 8 and len(skills) == 8, "V6八技能身份与主资产唯一性错误")
    require(m["manualReviewStatus"] == "PENDING" and
            m["releaseGate"] == "PENDING_ART_AND_GAMEPLAY_APPROVAL", "美术/玩法状态必须保持待审核")
    require(m["productionAbilityGrantCountChanged"] == 0, "不能通过UI改变服务器技能授予")
    for index, (row, draft) in enumerate(zip(rows, skills), 1):
        require(row["order"] == index and row["abilityId"] == draft["abilityId"] and
                row["slotId"] == draft["slotId"] and
                row["displayNameZh"] == draft["proposedNameZh"] and
                row["runtimeModeCandidate"] == reg[row["abilityId"]]["proposedPrimitiveCandidate"] and
                not reg[row["abilityId"]]["canGrantToServer"], "V6技能来源身份不符或未审批状态被修改")
    return m, rows


def soft_icon_path(entry):
    value = entry.get_editor_property("icon")
    # UE Python的TSoftObjectPtr代理因版本不同可能以SoftObjectPath/UTexture2D表达；
    # 只对GetPathName和字符串化做严格路径审核，绝不据此自动授予技能。
    if not value:
        return ""
    try:
        return str(value.get_path_name()).split(".")[0]
    except Exception:
        return str(value)


def run():
    m, rows = read_source()
    require(MODE in ("inspect", "apply"), "mode只能是inspect/apply")
    import unreal
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        ["/DBAHeroPack_Ox"], True)
    lib = unreal.EditorAssetLibrary
    profile = lib.load_asset(PROFILE_PATH)
    require(isinstance(profile, unreal.DivineBeastsAbilityUIProfile),
            "现有牛英雄唯一UIProfile必须为原生DataAsset")
    require(str(profile.get_editor_property("hero_definition_id")) == "Hero.Zodiac.Ox",
            "UIProfile主资产所属英雄错误")
    existing = list(profile.get_editor_property("entries"))
    idmap = {str(entry.get_editor_property("ability_id")): entry for entry in existing}
    require(len(idmap) == len(existing), "旧Profile已有重复AbilityId，拒绝新增")
    placeholder = {key for key in idmap if key.startswith("dba.ability.ox_placeholder_")}
    require(len(placeholder) >= 6, "不得覆盖或减少原牛英雄已授权占位技能配置")
    require(len(existing) + sum(row["abilityId"] not in idmap for row in rows) <= 64,
            "技能Profile超出64条上限")
    import_count = 0
    for row in rows:
        tex = lib.load_asset(row["targetTexturePath"])
        require(isinstance(tex, unreal.Texture2D),
                "原生Texture2D缺失：" + row["targetTexturePath"])
        require(tex.get_editor_property("lod_group") == unreal.TextureGroup.TEXTUREGROUP_UI,
                "新图标没有正确设置UI纹理分组")
        expected_id = row["abilityId"]
        if expected_id in idmap:
            entry = idmap[expected_id]
            actual_icon = soft_icon_path(entry)
            print("SSLM_OX_V6_PROFILE_PREEXISTING", expected_id,
                  str(entry.get_editor_property("display_name")),
                  actual_icon)
            require(str(entry.get_editor_property("display_name")) == row["displayNameZh"] and
                    row["targetTexturePath"] in actual_icon,
                    "已有同AbilityId展示映射与V6新规范不一致，拒绝覆盖：" + expected_id)
        else:
            import_count += 1
        print("SSLM_OX_V6_PROFILE_MAPPING_CHECKED", row["order"], row["displayNameZh"],
              expected_id, row["targetTexturePath"])
    print("SSLM_OX_V6_PROFILE_INSPECT_OK old", len(existing),
          "placeholders", len(placeholder), "add", import_count)
    if MODE == "inspect":
        return
    require("-SSLMOxV6ReviewOnlyProfileAuthoring" in unreal.SystemLibrary.get_command_line(),
            "必须显式审核展示作者命令行参数")
    require(Path(unreal.Paths.get_project_file_path()).resolve() ==
            (WORKSPACE / "Game/DivineBeastsArena.uproject").resolve(), "非正式工程，拒绝写资产")
    require(PROFILE_FILE.is_file(), "必须可备份原始UIProfile二进制资产")
    if import_count == 0:
        print("SSLM_OX_V6_PROFILE_ALREADY_BOUND 8")
        return

    new_entries = list(existing)
    for row in rows:
        if row["abilityId"] in idmap:
            continue
        tex = lib.load_asset(row["targetTexturePath"])
        item = unreal.DivineBeastsAbilityUIEntry()
        item.set_editor_property("ability_id", unreal.Name(row["abilityId"]))
        item.set_editor_property("display_name", unreal.Text(row["displayNameZh"]))
        item.set_editor_property("description", unreal.Text(row["mechanismZh"]))
        item.set_editor_property("icon", tex)
        new_entries.append(item)
    # 回滚所需的现有Profile在 Saved/Validation 保留，本次不覆盖他人相同备份。
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup = WORKSPACE / "Game/Saved/Validation/OxV6UIProfileBackup" / stamp
    backup.mkdir(parents=True, exist_ok=False)
    shutil.copy2(PROFILE_FILE, backup / PROFILE_FILE.name)
    print("SSLM_OX_V6_PROFILE_BACKUP", backup / PROFILE_FILE.name)
    profile.set_editor_property("entries", new_entries)
    require(lib.save_loaded_asset(profile, only_if_is_dirty=False),
            "真实UIProfile无法保存")
    reread = lib.load_asset(PROFILE_PATH)
    require(isinstance(reread, unreal.DivineBeastsAbilityUIProfile),
            "保存后UIProfile不再是合法类")
    final = list(reread.get_editor_property("entries"))
    known = {str(e.get_editor_property("ability_id")) for e in final}
    require(len(final) == len(existing) + import_count and
            set(placeholder).issubset(known) and
            all(row["abilityId"] in known for row in rows),
            "保存后V6展示数据丢失或原占位条目被删除")
    print("SSLM_OX_V6_PROFILE_REVIEW_ONLY_BOUND", import_count, "total", len(final),
          "grants_changed", 0, "art_approval", m["manualReviewStatus"])


if __name__ == "__main__":
    run()
