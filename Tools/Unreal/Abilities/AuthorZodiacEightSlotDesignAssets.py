# -*- coding: utf-8 -*-
"""SSLM十二生肖八槽技能策划数据资产制作入口。

此脚本只能通过已验证的UE5.8 Monolith MCP editor.run_python显式调用。
使用96条JSON策划真源写入正式项目的独立DesignOnly DataAsset以及96行DataTable。
禁止编译蓝图、修改Hero默认技能集合、创建假GameplayAbility、替换旧占位资产。
新增C++反射字段须先在用户单独批准的构建批次真实加载，否则明确拒绝写入。
"""
from __future__ import annotations
import json
import math
from pathlib import Path
import unreal

WORKSPACE = Path(__file__).resolve().parents[3]
GAME = WORKSPACE / "Game"
SOURCE = WORKSPACE / "Docs/Design/ZodiacEightSlotSkills_V2.json"
ASSET_ROOT = "/DBAGameplay/Abilities/EightSlotDrafts"
SLOT_ORDER = ("Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7")


def ensure(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError("SSLM八槽原生制作已拒绝：" + message)


def logical(value: str):
    namespace, last = value.rsplit(".", 1)
    name, version = last.rsplit("@", 1)
    result = unreal.GamePlatformId()
    result.set_editor_property("Namespace", namespace)
    result.set_editor_property("Name", name)
    result.set_editor_property("LogicalVersion", int(version))
    return result


def load_or_create(path: str, asset_class, factory):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        found = unreal.EditorAssetLibrary.load_asset(path)
        ensure(isinstance(found, asset_class), "已有其他类型资产，不允许覆盖：" + path)
        return found
    folder, name = path.rsplit("/", 1)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, asset_class, factory)
    ensure(isinstance(created, asset_class), "创建真实UE数据资产失败：" + path)
    return created


def asset_factory(asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("DataAssetClass", asset_class)
    return factory


def ui_tag(tag_name: str):
    tag = unreal.GameplayTag()
    ensure(tag.import_text(tag_name) and unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag),
           "Game/Config/DefaultGameplayTags.ini未登记技能输入：" + tag_name)
    return tag


def require_editor():
    cmd = unreal.SystemLibrary.get_command_line()
    ensure("-SSLMEightSlotDraftAuthoring" in cmd, "缺少-SSLMEightSlotDraftAuthoring显式制作开关")
    ensure(Path(unreal.Paths.get_project_file_path()).resolve() ==
           (GAME / "DivineBeastsArena.uproject").resolve(), "非本SSLM正式工程")
    ensure("5.8" in unreal.SystemLibrary.get_engine_version(), "必须使用锁定UE5.8")
    ensure(hasattr(unreal, "DivineBeastsEightSlotDraftAction"),
           "未加载新增结构FDivineBeastsEightSlotDraftAction；禁止在旧DLL上写丢字段的资产")
    ensure(unreal.load_object(None,
           "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow") is not None,
           "未加载双端安全数值表结构")


def safe_catalog():
    raw = json.loads(SOURCE.read_text(encoding="utf-8"))
    ensure(raw["schemaVersion"] == 2 and raw["skillCount"] == 96 and raw["heroCount"] == 12,
           "策划Schema版本或英雄／技能数量非法")
    ensure(raw["designerApprovalStatus"] == "PENDING_HUMAN_BALANCE_REVIEW",
           "正式批准状态已变化，禁止继续用草案作者覆盖")
    ensure(raw.get("balanceStatus") == "LEVEL1_CANDIDATE_NOT_BALANCED",
           "一级数值未通过平衡审批，不允许使用错误状态的设计稿写入资产")
    ensure(len(raw["heroes"]) == 12, "必须是12位生肖")
    ids = set()
    for hero in raw["heroes"]:
        ensure(len(hero["slots"]) == 8, hero["heroNameZh"] + "不是八个有效技能")
        ensure(tuple(item["slotId"] for item in hero["slots"]) == SLOT_ORDER,
               hero["heroNameZh"] + "技能槽或输入顺序错误")
        current_hero = unreal.EditorAssetLibrary.load_asset(
            "/DBAGameplay/Definitions/DA_Hero_Zodiac_" + hero["heroId"].split(".")[-1])
        ensure(isinstance(current_hero, unreal.DivineBeastsHeroDefinition),
               "原英雄定义缺失：" + hero["heroId"])
        # 只读取原DefaultAbilitySetId；绝不写回Hero或建立可立即授权的新AbilitySet。
        for row in hero["slots"]:
            identity = row["abilityId"]
            ensure(identity not in ids and identity.startswith("dba.ability."),
                   "重复技能身份：" + identity)
            ids.add(identity)
            ui_tag(next(x["tag"] for x in raw["slotPolicy"]["inputSemantics"]
                        if x["slot"] == row["slotId"]))
            ensure(row["implementationStatus"] == "DESIGN_ONLY_UNIMPLEMENTED" and
                   not row["approvedForLiveGAS"], "草案混入可上线技能：" + identity)
            ensure(len(row["actions"]) >= 1, "缺少正式技能设计行为：" + identity)
            ensure(all(math.isfinite(float(x)) and float(x) >= 0 for x in (
                row["baseDamage"], row["cooldownSeconds"], row["momentumCost"],
                row["castRangeCm"], row["areaRadiusCm"])),
                "伤害、冷却或范围存在无效数值：" + identity)
            ensure(row["damageType"] != "Magic" or row["momentumCost"] == 0,
                   "Magic法术不能扣气势：" + identity)
            ensure(all(action["kind"] != "damage" or
                       action.get("type") == row["damageType"]
                       for action in row["actions"]),
                   "伤害行为类型与数据表类型不一致：" + identity)
            has_damage = any(action["kind"] == "damage" for action in row["actions"])
            has_heal = any(action["kind"] == "healing" for action in row["actions"])
            ensure(not (has_damage and has_heal) or row["intentTarget"] == "area_center_mixed",
                   "伤害和治疗混合技能必须使用范围中心目标身份：" + identity)
    ensure(len(ids) == 96, "本轮技能身份不是96个")
    return raw



def preflight_existing_destinations(skill_inputs, row_names):
    """任何写盘前一次性核查97个目标，杜绝中途发现同名正式资产而覆盖已有作者成果。"""
    table_path = ASSET_ROOT + "/DT_DBA_ZodiacEightSlotDraftBalance"
    if unreal.EditorAssetLibrary.does_asset_exist(table_path):
        existing_table = unreal.EditorAssetLibrary.load_asset(table_path)
        ensure(isinstance(existing_table, unreal.DataTable),
               "草案数值表路径已有非DataTable资产：" + table_path)
        try:
            old_names = {str(name) for name in unreal.DataTableFunctionLibrary.get_data_table_row_names(existing_table)}
        except Exception as error:
            raise RuntimeError("无法核验已有草案数值表的行名，禁止覆盖") from error
        ensure(not old_names or old_names == row_names,
               "已有DataTable的行身份与96项设计源不一致；必须先单独审批迁移")
    for hero, row, _ in skill_inputs:
        hero_name = hero["heroId"].split(".")[-1]
        key = "".join(segment.title() for segment in row["abilityKey"].split("_"))
        path = ASSET_ROOT + "/DA_DBA_" + hero_name + "_Eight_" + key
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        existing = unreal.EditorAssetLibrary.load_asset(path)
        ensure(isinstance(existing, unreal.DivineBeastsAbilityDefinition),
               "已有同名资产并非项目技能DataAsset：" + path)
        ensure(bool(existing.get_editor_property("bEightSlotDraftOnly")),
               "已有同名资产不是受控的八槽设计稿，禁止覆盖：" + path)
        ensure(existing.get_editor_property("LogicalId").export_text() ==
               logical(row["abilityId"]).export_text(),
               "同名设计稿的稳定LogicalId不同，禁止接管：" + path)


def author():
    require_editor()
    source = safe_catalog()
    kind_class = unreal.DivineBeastsAbilityDefinition
    design_cdo = unreal.get_default_object(kind_class)
    ensure(hasattr(design_cdo, "get_editor_property"), "DataAsset反射不能读取")
    try:
        design_cdo.get_editor_property("bEightSlotDraftOnly")
    except Exception as error:
        raise RuntimeError("新C++扩展DataAsset字段未由当前Editor加载，停止制作") from error

    # 预先计算96行与所有目标路径，先发现冲突再创建任何真实UE资源。
    rows, skill_inputs = [], []
    for hero in source["heroes"]:
        for entry in hero["slots"]:
            name = entry["abilityId"].rsplit(".", 1)[-1].removesuffix("@1") + "_L1"
            rows.append({
                "Name": name, "AbilityId": entry["abilityId"], "Level": 1,
                "BaseDamage": entry["baseDamage"],
                "DamageType": entry["damageType"],
                "CooldownSeconds": entry["cooldownSeconds"],
                "MomentumCost": entry["momentumCost"],
                "CastRangeCm": entry["castRangeCm"],
                "AreaRadiusCm": entry["areaRadiusCm"]
            })
            skill_inputs.append((hero, entry, name))
    ensure(len(rows) == 96, "DataTable须96行")
    preflight_existing_destinations(skill_inputs, {row["Name"] for row in rows})
    factory = unreal.DataTableFactory()
    factory.set_editor_property("Struct", unreal.load_object(
        None, "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow"))
    table = load_or_create(ASSET_ROOT + "/DT_DBA_ZodiacEightSlotDraftBalance",
                           unreal.DataTable, factory)
    ensure(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(rows, ensure_ascii=False)),
        "96行数据表原生引擎导入失败，停止进一步制作")
    ensure(unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False),
           "数值DataTable保存失败")
    saved = [table.get_path_name()]
    for hero, row, table_row_name in skill_inputs:
        hero_name = hero["heroId"].split(".")[-1]
        key = "".join(segment.title() for segment in row["abilityKey"].split("_"))
        path = ASSET_ROOT + "/DA_DBA_" + hero_name + "_Eight_" + key
        definition = load_or_create(path, kind_class, asset_factory(kind_class))
        # 既有数据资产身份已在任何写盘之前统一预检，不能在中途静默占用正式技能。
        definition.set_editor_property("LogicalId", logical(row["abilityId"]))
        definition.set_editor_property("HeroDefinitionId", unreal.Name(hero["heroId"]))
        kind = "PRIMARY" if row["role"] == "basic" else (
            "ULTIMATE" if row["role"] == "ultimate" else "ACTIVE")
        definition.set_editor_property("Kind", getattr(unreal.DivineBeastsAbilityKind, kind))
        definition.set_editor_property("BalanceTable", table)
        definition.set_editor_property("LevelRowNames", [table_row_name])
        definition.set_editor_property("bEightSlotDraftOnly", True)
        definition.set_editor_property("DraftSlotId", unreal.Name(row["slotId"]))
        input_tag = next(x["tag"] for x in source["slotPolicy"]["inputSemantics"]
                         if x["slot"] == row["slotId"])
        definition.set_editor_property("DraftInputTag", ui_tag(input_tag))
        definition.set_editor_property("DraftDisplayNameZh", unreal.Text(row["nameZh"]))
        definition.set_editor_property("DraftDescriptionZh", unreal.Text(row["descriptionZh"]))
        definition.set_editor_property("DraftRoleId", unreal.Name(row["role"]))
        definition.set_editor_property("DraftTargetModeId", unreal.Name(row["intentTarget"]))
        definition.set_editor_property("DraftHeroVFXProfileId", unreal.Name(row["vfx"]["heroProfileId"]))
        definition.set_editor_property("DraftPresentationSemanticId",
                                       unreal.Name(row["vfx"]["phaseSemantics"][0]))
        actions = []
        for row_action in row["actions"]:
            action = unreal.DivineBeastsEightSlotDraftAction()
            action.set_editor_property("ActionKindId", unreal.Name(row_action["kind"]))
            modifier = row_action.get("controlType") or row_action.get("buffType") or ""
            action.set_editor_property("ModifierId", unreal.Name(modifier))
            magnitude = (row_action.get("baseMagnitude")
                         if "baseMagnitude" in row_action else
                         row_action.get("distanceCm", row_action.get("maxHealthFraction",
                         row_action.get("magnitudeFraction", row_action.get("slowFraction", 0.0)))))
            action.set_editor_property("Magnitude", float(magnitude))
            action.set_editor_property("DurationSeconds", float(row_action.get("durationSeconds", 0.0)))
            action.set_editor_property("HitCount", int(row_action.get("hitCount", 1)))
            actions.append(action)
        definition.set_editor_property("DraftActions", actions)
        # ValidateDefinition是项目C++非UFUNCTION方法，Python不能把未公开方法调用冒充原生验证。
        # 保存前只回读作者字段；UE原生DataValidation与重新打开进程冷读另行留证。
        ensure(bool(definition.get_editor_property("bEightSlotDraftOnly")) and
               len(definition.get_editor_property("DraftActions")) == len(actions),
               "作者DataAsset字段回读失败：" + row["abilityId"])
        ensure(unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False),
               "写盘失败：" + row["abilityId"])
        saved.append(definition.get_path_name())
    # 仅返回真实成功包，96份作者稿不代表GA、VFX、8槽Widget已经完成。
    unreal.log("SSLM八槽作者已保存真实DataAsset=" + str(len(saved)) +
               "（96定义+1表），Hero DefaultAbilitySetId保持未改。")
    return {"Status": "AuthoringSavedNeedsNativeReadback", "SavedPaths": saved,
            "AbilityDefinitions": 96, "BalanceRows": 96, "HeroDefaultsChanged": 0,
            "NativeGASRuntimeVerified": False, "VFXAssetsVerified": False,
            "BlueprintEightSlotsVerified": False}


# Monolith MCP外部调用器只能显式执行author()；import模块或打开编辑器不会发生资产改写。
