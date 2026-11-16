# -*- coding: utf-8 -*-
"""SSLM V6原生技能*草案*数据资产制作。必须在正式UE5.8编辑器内由Monolith显式调用。

只生成96项待审核定义与一级DataTable，不创建空GA、GE、AbilitySet，不替换正式Hero，
也不清除bEightSlotDraftOnly；有任一同名冲突先整体拒绝，不把写到一半称为完成。
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "Game/DivineBeastsArena.uproject"
REGISTRY = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json"
DESIGN = ROOT / "Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json"
ASSET_ROOT = "/DBAGameplay/Abilities/V6DraftReview"
TABLE_PATH = ASSET_ROOT + "/DT_DBA_V6DraftBalance"
INPUTS = {
    "Primary": "Platform.Ability.Input.DivineBeasts.Primary",
    "Slot1": "Platform.Ability.Input.DivineBeasts.Slot1",
    "Slot2": "Platform.Ability.Input.DivineBeasts.Slot2",
    "Slot4": "Platform.Ability.Input.DivineBeasts.Slot4",
    "Slot3": "Platform.Ability.Input.DivineBeasts.Slot3",
    "Slot5": "Platform.Ability.Input.DivineBeasts.Slot5",
    "Slot6": "Platform.Ability.Input.DivineBeasts.Slot6",
    "Slot7": "Platform.Ability.Input.DivineBeasts.Slot7",
}


def require(condition, reason):
    if not condition:
        raise RuntimeError("SSLM V6 DataAsset作者拒绝：" + reason)


def logical_id(value):
    namespace, name_version = value.rsplit(".", 1)
    name, version = name_version.rsplit("@", 1)
    result = unreal.GamePlatformId()
    result.set_editor_property("Namespace", namespace)
    result.set_editor_property("Name", name)
    result.set_editor_property("LogicalVersion", int(version))
    return result


def native_asset_path(skill):
    stable_name = skill["abilityId"].rsplit(".", 1)[-1].removesuffix("@1")
    return ASSET_ROOT + "/DA_DBA_V6_" + stable_name


def safe_catalog():
    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    design = json.loads(DESIGN.read_text(encoding="utf-8"))
    skills = registry.get("skills", [])
    require(registry.get("skillCount") == 96 and len(skills) == 96,
            "台账不是96条V6技能")
    require(registry.get("status") == "SOURCE_IMPLEMENTATION_UNCOMPILED_NOT_GRANTED" and
            registry.get("serverApprovedAbilityCount") == 0,
            "本作者只允许未批准的独立审核版本，禁止把正式授权状态写回草案")
    require(design.get("status") == "PENDING_HUMAN_APPROVAL",
            "方案已非待审核状态，不可用旧草案作者覆盖")
    require(len({x["abilityId"] for x in skills}) == 96, "技能稳定身份有重复")
    expected = {
        item["abilityId"]: (hero, item)
        for hero in design["heroes"] for item in hero["skills"]
    }
    require(len(expected) == 96, "历史设计源技能数量不合法")
    for skill in skills:
        hero, definition = expected.get(skill["abilityId"], (None, None))
        require(hero is not None and definition is not None and
                hero["heroId"] == skill["heroId"] and
                skill["displayNameV6"] == definition["proposedNameZh"] and
                not skill["canGrantToServer"] and
                skill["reviewState"] == "PENDING_USER_REVIEW",
                "V6定义、技能名称或签审状态不一致：" + skill["abilityId"])
        require(definition["slotId"] in INPUTS, "技能输入槽不属于八槽")
        values = skill["level1Candidate"]
        require(all(math.isfinite(float(values[field])) and float(values[field]) >= 0
                    for field in ("baseDamage", "cooldownSeconds", "momentumCost",
                                  "castRangeCm", "areaRadiusCm")),
                "V2一级候选数值无效：" + skill["abilityId"])
        require(values["damageType"] != "Magic" or values["momentumCost"] == 0,
                "禁止法术消耗气势")
        require(len(skill["v2Actions"]) >= 1, "缺少V2动作预审参数")

    # 仅制作待审核DataTable时应用明确记录的V6候选修正；不得悄悄改V2源表或已运行技能。
    fixes = {}
    index = {skill["abilityId"]: skill for skill in skills}
    for correction in registry.get("v6DraftBalanceCorrections", []):
        identity = correction.get("abilityId", "")
        require(identity in index and identity not in fixes,
                "V6候选纠错引用不存在或重复的技能：" + identity)
        skill = index[identity]
        actions = skill.get("v2Actions", [])
        require(skill["proposedPrimitiveCandidate"] == "SafeDash" and len(actions) == 1 and
                actions[0].get("kind") == "movement" and
                correction.get("status") == "PENDING_DESIGN_REVIEW" and
                correction.get("approvedForRuntime") is False and
                float(correction["sourceCandidateCastRangeCm"]) ==
                    float(skill["level1Candidate"]["castRangeCm"]) and
                float(correction["actionTravelDistanceCm"]) ==
                    float(actions[0].get("distanceCm", -1)) and
                float(actions[0]["distanceCm"]) <= float(correction["proposedCastRangeCm"]) <= 2000,
                "V6位移候选修正未满足真实性、审批或射程边界：" + identity)
        fixes[identity] = correction
    return skills, expected, fixes


def check_editor_and_preflight(skills):
    # 对每次制作显式复核引擎进程和插件反射类型；不会自动启动编辑器／编译。
    require("-SSLMV6DraftAssetAuthoring" in unreal.SystemLibrary.get_command_line(),
            "缺少制作权限开关 -SSLMV6DraftAssetAuthoring")
    require(Path(unreal.Paths.get_project_file_path()).resolve() == PROJECT.resolve(),
            "当前不是正式DivineBeastsArena工程")
    require("5.8" in unreal.SystemLibrary.get_engine_version(), "不是锁定UE5.8")
    native_type = unreal.DivineBeastsAbilityDefinition
    cdo = unreal.get_default_object(native_type)
    for field in ("bEightSlotDraftOnly", "bV6NativeApproved", "DraftSlotId",
                  "DraftInputTag", "DraftActions", "DraftDisplayNameZh",
                  "DraftDescriptionZh"):
        try:
            cdo.get_editor_property(field)
        except Exception as exc:
            raise RuntimeError("当前Editor没有最新V6反射属性：" + field) from exc

    all_paths = [native_asset_path(x) for x in skills]
    require(len(set(all_paths)) == 96, "目标资产名发生碰撞")
    if unreal.EditorAssetLibrary.does_asset_exist(TABLE_PATH):
        raise RuntimeError("V6目标DataTable已存在；必须先人工核对历史版本再迁移，拒绝覆盖")
    for path in all_paths:
        require(not unreal.EditorAssetLibrary.does_asset_exist(path),
                "已存在同名技能资产，拒绝覆盖：" + path)
    # 本次绝不创建/替换旧占位、V2草案、Hero主定义或GA。
    return native_type


def make_assets():
    """仅供正式Monolith MCP人工选定调用。没有连接成功绝不自动执行。"""
    skills, expected, pending_fixes = safe_catalog()
    native_type = check_editor_and_preflight(skills)
    struct = unreal.load_object(None,
        "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow")
    require(struct is not None, "未加载FDivineBeastsAbilityBalanceRow反射结构")
    table_factory = unreal.DataTableFactory()
    table_factory.set_editor_property("Struct", struct)
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DT_DBA_V6DraftBalance", ASSET_ROOT, unreal.DataTable, table_factory)
    require(table is not None, "无法创建V6一级DataTable")

    balance_rows = []
    for skill in skills:
        value = skill["level1Candidate"]
        correction = pending_fixes.get(skill["abilityId"])
        draft_range_cm = (float(correction["proposedCastRangeCm"])
                          if correction else float(value["castRangeCm"]))
        balance_rows.append({
            "Name": skill["abilityId"].rsplit(".", 1)[-1].removesuffix("@1") + "_L1",
            "AbilityId": skill["abilityId"], "Level": 1,
            "BaseDamage": value["baseDamage"], "DamageType": value["damageType"],
            "CooldownSeconds": value["cooldownSeconds"],
            "MomentumCost": value["momentumCost"],
            # 草案纠错仍不能使V6技能获得GAS批准；必须在人工确认后另建正式多级数值表。
            "CastRangeCm": draft_range_cm, "AreaRadiusCm": value["areaRadiusCm"],
        })
    require(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(balance_rows, ensure_ascii=False)),
        "V6数值表原生导入失败")
    require(unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False),
            "V6数值表写盘失败")
    saved_paths = [TABLE_PATH]

    data_factory = unreal.DataAssetFactory()
    data_factory.set_editor_property("DataAssetClass", native_type)
    for skill in skills:
        hero, design = expected[skill["abilityId"]]
        path = native_asset_path(skill)
        _, name = path.rsplit("/", 1)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, ASSET_ROOT, native_type, data_factory)
        require(asset is not None, "真实V6数据资产创建失败：" + path)
        asset.set_editor_property("LogicalId", logical_id(skill["abilityId"]))
        asset.set_editor_property("HeroDefinitionId", unreal.Name(skill["heroId"]))
        slot = skill["slotId"]
        kind = "PRIMARY" if slot == "Primary" else "ULTIMATE" if slot == "Slot4" else "ACTIVE"
        asset.set_editor_property("Kind", getattr(unreal.DivineBeastsAbilityKind, kind))
        asset.set_editor_property("BalanceTable", table)
        asset.set_editor_property("LevelRowNames", [
            skill["abilityId"].rsplit(".", 1)[-1].removesuffix("@1") + "_L1"
        ])
        asset.set_editor_property("bEightSlotDraftOnly", True)
        asset.set_editor_property("bV6NativeApproved", False)
        asset.set_editor_property("DraftSlotId", unreal.Name(slot))
        tag = unreal.GameplayTag()
        require(tag.import_text(INPUTS[slot]), "真实GAS标签尚未注册：" + INPUTS[slot])
        asset.set_editor_property("DraftInputTag", tag)
        asset.set_editor_property("DraftDisplayNameZh", unreal.Text(skill["displayNameV6"]))
        asset.set_editor_property("DraftDescriptionZh", unreal.Text(
            "V6历史技能待审核：" + design["descriptionZh"] +
            "。当前DataTable仅包含V2一级候选，不能当成V6实际结算。"))
        asset.set_editor_property("DraftRoleId", unreal.Name(design["roleZh"]))
        # 历史V6的复合目标行为不自动映射成单一服务器模式。
        asset.set_editor_property("DraftTargetModeId", unreal.Name("v6_pending_target_review"))
        asset.set_editor_property("DraftHeroVFXProfileId", unreal.Name(
            "Presentation.VFX.Hero.Zodiac." + skill["heroId"].rsplit(".", 1)[-1] + ".Default"))
        asset.set_editor_property("DraftPresentationSemanticId", unreal.Name(
            "Presentation.DBA.V6." + skill["abilityId"].rsplit(".", 1)[-1].split("@")[0] + ".Cast"))

        action_list = []
        for row in skill["v2Actions"]:
            action = unreal.DivineBeastsEightSlotDraftAction()
            action.set_editor_property("ActionKindId", unreal.Name(row["kind"]))
            action.set_editor_property("ModifierId", unreal.Name(
                row.get("controlType") or row.get("buffType") or ""))
            magnitude = (row.get("baseMagnitude") if "baseMagnitude" in row else
                         row.get("distanceCm", row.get("maxHealthFraction",
                         row.get("magnitudeFraction", row.get("slowFraction", 0.0)))))
            action.set_editor_property("Magnitude", float(magnitude))
            action.set_editor_property("DurationSeconds", float(row.get("durationSeconds", 0.0)))
            action.set_editor_property("HitCount", int(row.get("hitCount", 1)))
            action_list.append(action)
        asset.set_editor_property("DraftActions", action_list)
        require(asset.get_editor_property("bEightSlotDraftOnly") and
                not asset.get_editor_property("bV6NativeApproved") and
                len(asset.get_editor_property("DraftActions")) == len(action_list),
                "原生资产反射字段没有正确回读：" + path)
        require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False),
                "技能草案写盘失败：" + path)
        saved_paths.append(path)
    # 这些保存只代表真实设计DataAsset完成，不会授权GameplayAbility。
    unreal.log("SSLM V6已制作未批准DataAsset: " + str(len(saved_paths)))
    return {
        "Status": "UNAPPROVED_NATIVE_DRAFTS_REQUIRE_REOPEN_VALIDATION",
        "SavedPaths": saved_paths, "Definitions": 96, "BalanceRows": 96,
        "GameplayAbilitiesGranted": 0, "HeroDefaultsChanged": 0,
        "BlueprintsCompiled": False, "VFXApproved": False,
    }


# 禁止导入模块时自动改资产；必须显式由连接到此工程的Monolith MCP调用make_assets()。
