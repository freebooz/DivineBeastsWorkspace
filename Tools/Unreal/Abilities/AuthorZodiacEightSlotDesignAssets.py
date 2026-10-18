# -*- coding: utf-8 -*-
"""SSLM十二生肖八槽技能策划数据资产制作入口。

只能由已核验的UE5.8 Monolith MCP显式调用author()。
先将96项JSON设计与96行CSV数值建立严格契约，再写入DBAGameplay自有的隔离草案目录。
不会编译、修改英雄DefaultAbilitySetId、创建GameplayAbility或伪造动画/VFX/UI资源。
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
from ZodiacEightSlotDraftContract import (  # noqa: E402
    ASSET_OWNER_ID,
    ASSET_ROOT,
    CATALOG_ID,
    TABLE_OBJECT_PATH,
    build_contract,
)

WORKSPACE = Path(__file__).resolve().parents[3]
GAME = WORKSPACE / "Game"
SOURCE = WORKSPACE / "Docs/Design/ZodiacEightSlotSkills_V2.json"
BALANCE_SOURCE = WORKSPACE / (
    "Game/Plugins/DivineBeasts/DBAGameplay/SourceArt/Abilities/"
    "DT_DBA_ZodiacEightSlotDraftBalance_V2.csv"
)
ABILITY_CLASS_PATH = "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityDefinition"
BALANCE_STRUCT_PATH = "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow"
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


def logical_text(value) -> str:
    try:
        namespace = str(value.get_editor_property("Namespace"))
        name = str(value.get_editor_property("Name"))
        version = int(value.get_editor_property("LogicalVersion"))
    except Exception as error:
        raise RuntimeError("平台逻辑ID反射不可读") from error
    return namespace + "." + name + "@" + str(version)


def asset_exists(path: str) -> bool:
    ensure(path.startswith(ASSET_ROOT + "/"), "拒绝访问隔离草案根目录之外的资产：" + path)
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def load_existing(path: str, asset_class):
    if not asset_exists(path):
        return None
    found = unreal.EditorAssetLibrary.load_asset(path)
    ensure(isinstance(found, asset_class), "目标对象已存在但资产类别不符，拒绝覆盖：" + path)
    ensure(found.get_path_name() == path, "引擎返回的对象路径不匹配：" + path)
    return found


def load_or_create(path: str, asset_class, factory):
    existing = load_existing(path, asset_class)
    if existing is not None:
        return existing, False
    folder, name = path.rsplit("/", 1)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, asset_class, factory
    )
    ensure(isinstance(created, asset_class), "创建真实UE数据资产失败：" + path)
    ensure(created.get_path_name() == path, "创建结果不在隔离草案路径：" + path)
    return created, True


def asset_factory(asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("DataAssetClass", asset_class)
    return factory


def ui_tag(tag_name: str):
    tag = unreal.GameplayTag()
    ensure(
        tag.import_text(tag_name) and unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag),
        "Game/Config/DefaultGameplayTags.ini未登记技能输入：" + tag_name,
    )
    return tag


def require_editor():
    command_line = unreal.SystemLibrary.get_command_line()
    ensure("-SSLMEightSlotDraftAuthoring" in command_line,
           "缺少-SSLMEightSlotDraftAuthoring显式制作开关")
    project_file = Path(unreal.Paths.get_project_file_path()).resolve()
    ensure(project_file == (GAME / "DivineBeastsArena.uproject").resolve(),
           "当前不是SSLM正式工程")
    ensure("5.8" in unreal.SystemLibrary.get_engine_version(), "必须使用锁定UE5.8")
    ensure(hasattr(unreal, "DivineBeastsEightSlotDraftAction"),
           "未加载扩展行为结构；禁止在旧Editor/DLL上写入不完整资产")
    ability_class = unreal.load_object(None, ABILITY_CLASS_PATH)
    balance_struct = unreal.load_object(None, BALANCE_STRUCT_PATH)
    ensure(ability_class is not None and balance_struct is not None,
           "新增能力定义或安全数值行反射类尚未由当前Editor加载")

    ability_cdo = unreal.get_default_object(ability_class)
    ability_properties = (
        "LogicalId", "HeroDefinitionId", "Kind", "BalanceTable", "LevelRowNames",
        "bEightSlotDraftOnly", "DraftSlotId", "DraftInputTag", "DraftInputActionId",
        "DraftDefaultKeyId", "DraftDisplayOrder", "DraftDisplayNameZh",
        "DraftDescriptionZh", "DraftRoleId", "DraftTargetModeId", "DraftActions",
        "DraftAssetOwnerId", "DraftCatalogId", "DraftSourceSha256",
        "DraftHeroVFXProfileId", "DraftVFXCatalogKey",
        "DraftPresentationSemanticId", "DraftPresentationSemanticIds",
    )
    for property_name in ability_properties:
        try:
            ability_cdo.get_editor_property(property_name)
        except Exception as error:
            raise RuntimeError("当前Editor缺少DataAsset反射字段：" + property_name) from error

    action_probe = unreal.DivineBeastsEightSlotDraftAction()
    for property_name in (
        "ActionKindId", "ModifierId", "MagnitudeSemanticId", "DamageTypeId",
        "Magnitude", "DurationSeconds", "HitCount", "bServerNavValidation",
        "bAllowRevive", "SourcePayloadJson",
    ):
        try:
            action_probe.get_editor_property(property_name)
        except Exception as error:
            raise RuntimeError("当前Editor缺少设计行为反射字段：" + property_name) from error


def safe_catalog():
    catalog = json.loads(SOURCE.read_text(encoding="utf-8-sig"))
    balance_csv = BALANCE_SOURCE.read_text(encoding="utf-8-sig")
    contract = build_contract(catalog, balance_csv)

    for hero in catalog["heroes"]:
        hero_path = "/DBAGameplay/Definitions/DA_Hero_Zodiac_" + hero["heroId"].rsplit(".", 1)[-1]
        current_hero = unreal.EditorAssetLibrary.load_asset(hero_path)
        ensure(isinstance(current_hero, unreal.DivineBeastsHeroDefinition),
               "原英雄定义缺失或类别不符：" + hero["heroId"])
        expected_placeholder_id = f"dba.abilityset.{hero['heroCode']}_placeholder@1"
        default_set_id = current_hero.get_editor_property("DefaultAbilitySetId")
        ensure(logical_text(default_set_id) == expected_placeholder_id,
               "英雄正式占位技能集已变化，停止自动写入：" + hero["heroId"])
        for entry in hero["slots"]:
            input_row = next(x for x in catalog["slotPolicy"]["inputSemantics"]
                             if x["slot"] == entry["slotId"])
            ui_tag(input_row["tag"])
    return catalog, contract


def preflight_existing_assets(contract: dict, ability_class, balance_struct):
    """写入前扫描97个目标；任何越权、冲突或旧正式资产均失败关闭。"""
    existing_table = load_existing(TABLE_OBJECT_PATH, unreal.DataTable)
    if existing_table is not None:
        ensure(existing_table.get_row_struct() == balance_struct,
               "数值表已有资产的行结构不是当前UE5.8能力行")
    existing_definitions = {}
    for skill in contract["skills"]:
        path = skill["asset_path"]
        existing = load_existing(path, ability_class)
        if existing is not None:
            ensure(bool(existing.get_editor_property("bEightSlotDraftOnly")),
                   "目标路径已有非草案正式定义，拒绝接管：" + path)
            ensure(str(existing.get_editor_property("DraftAssetOwnerId")) == ASSET_OWNER_ID,
                   "已有技能资产不属于DBAGameplay：" + path)
            ensure(str(existing.get_editor_property("DraftCatalogId")) == CATALOG_ID,
                   "已有技能资产属于其他设计目录：" + path)
            ensure(logical_text(existing.get_editor_property("LogicalId")) ==
                   skill["entry"]["abilityId"], "草案对象稳定身份冲突：" + path)
            ensure(str(existing.get_editor_property("HeroDefinitionId")) ==
                   skill["hero"]["heroId"], "草案对象英雄归属冲突：" + path)
            ensure(str(existing.get_editor_property("DraftSlotId")) ==
                   skill["entry"]["slotId"], "草案对象输入槽归属冲突：" + path)
        existing_definitions[path] = existing
    return existing_table, existing_definitions


def make_action(mapped: dict):
    action = unreal.DivineBeastsEightSlotDraftAction()
    for property_name in (
        "ActionKindId", "ModifierId", "MagnitudeSemanticId", "DamageTypeId",
        "Magnitude", "DurationSeconds", "HitCount", "bServerNavValidation",
        "bAllowRevive", "SourcePayloadJson",
    ):
        value = mapped[property_name]
        if property_name in ("ActionKindId", "ModifierId", "MagnitudeSemanticId", "DamageTypeId"):
            value = unreal.Name(value)
        action.set_editor_property(property_name, value)
    return action


def author():
    require_editor()
    catalog, contract = safe_catalog()
    ability_class = unreal.load_object(None, ABILITY_CLASS_PATH)
    balance_struct = unreal.load_object(None, BALANCE_STRUCT_PATH)
    existing_table, existing_definitions = preflight_existing_assets(
        contract, ability_class, balance_struct
    )

    table_factory = unreal.DataTableFactory()
    table_factory.set_editor_property("Struct", balance_struct)
    table, table_created = load_or_create(
        TABLE_OBJECT_PATH, unreal.DataTable, table_factory
    )
    ensure(table.get_row_struct() == balance_struct, "数值表真实行结构不匹配")
    ensure(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(contract["balanceRows"], ensure_ascii=False)
    ), "96行数值数据表导入失败")
    ensure(unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False),
           "数值DataTable保存失败")

    saved_paths = [table.get_path_name()]
    created_count = 0
    updated_count = 0
    for skill in contract["skills"]:
        hero = skill["hero"]
        entry = skill["entry"]
        path = skill["asset_path"]
        definition = existing_definitions[path]
        if definition is None:
            definition, created = load_or_create(
                path, ability_class, asset_factory(ability_class)
            )
            ensure(created, "资产在预检后意外出现；停止并重新检查并行写入：" + path)
            created_count += 1
        else:
            updated_count += 1

        definition.set_editor_property("LogicalId", logical(entry["abilityId"]))
        definition.set_editor_property("HeroDefinitionId", unreal.Name(hero["heroId"]))
        kind_name = "PRIMARY" if entry["role"] == "basic" else (
            "ULTIMATE" if entry["role"] == "ultimate" else "ACTIVE"
        )
        definition.set_editor_property(
            "Kind", getattr(unreal.DivineBeastsAbilityKind, kind_name)
        )
        definition.set_editor_property("BalanceTable", table)
        definition.set_editor_property("LevelRowNames", [unreal.Name(skill["row_name"])])
        definition.set_editor_property("bEightSlotDraftOnly", True)
        definition.set_editor_property("DraftSlotId", unreal.Name(entry["slotId"]))
        definition.set_editor_property("DraftInputTag", ui_tag(skill["input"]["tag"]))
        definition.set_editor_property("DraftInputActionId", unreal.Name(skill["input"]["action"]))
        definition.set_editor_property("DraftDefaultKeyId", unreal.Name(skill["input"]["keyboard"]))
        definition.set_editor_property("DraftDisplayOrder", int(entry["displayOrder"]))
        definition.set_editor_property("DraftDisplayNameZh", unreal.Text(entry["nameZh"]))
        definition.set_editor_property("DraftDescriptionZh", unreal.Text(entry["descriptionZh"]))
        definition.set_editor_property("DraftRoleId", unreal.Name(entry["role"]))
        definition.set_editor_property("DraftTargetModeId", unreal.Name(entry["intentTarget"]))
        definition.set_editor_property("DraftAssetOwnerId", unreal.Name(ASSET_OWNER_ID))
        definition.set_editor_property("DraftCatalogId", unreal.Name(CATALOG_ID))
        definition.set_editor_property("DraftSourceSha256", skill["source_sha256"])
        definition.set_editor_property(
            "DraftHeroVFXProfileId", unreal.Name(entry["vfx"]["heroProfileId"])
        )
        definition.set_editor_property(
            "DraftVFXCatalogKey", unreal.Name(entry["vfx"]["catalogKey"])
        )
        phase_ids = [unreal.Name(value) for value in entry["vfx"]["phaseSemantics"]]
        definition.set_editor_property("DraftPresentationSemanticId", phase_ids[0])
        definition.set_editor_property("DraftPresentationSemanticIds", phase_ids)
        definition.set_editor_property(
            "DraftActions", [make_action(action) for action in skill["actions"]]
        )

        # C++ValidateDefinition不是UFUNCTION，Python不冒充原生校验；这里仅核对每个反射字段回读。
        ensure(logical_text(definition.get_editor_property("LogicalId")) ==
               entry["abilityId"], "逻辑技能身份回读失败：" + entry["abilityId"])
        ensure(str(definition.get_editor_property("HeroDefinitionId")) == hero["heroId"] and
               str(definition.get_editor_property("DraftSlotId")) == entry["slotId"],
               "英雄或输入槽归属回读失败：" + entry["abilityId"])
        ensure(str(definition.get_editor_property("DraftInputTag")) == skill["input"]["tag"],
               "GAS输入标签回读失败：" + entry["abilityId"])
        ensure(str(definition.get_editor_property("DraftSourceSha256")) == skill["source_sha256"],
               "源设计摘要回读失败：" + entry["abilityId"])
        ensure(len(definition.get_editor_property("DraftActions")) == len(skill["actions"]),
               "策划行为回读失败：" + entry["abilityId"])
        ensure(bool(definition.get_editor_property("bEightSlotDraftOnly")),
               "草案禁用位回读失败：" + entry["abilityId"])
        ensure(unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False),
               "DataAsset保存失败：" + entry["abilityId"])
        saved_paths.append(definition.get_path_name())

    expected_asset_paths = {skill["asset_path"] for skill in contract["skills"]}
    ensure(len(saved_paths) == 97 and len(expected_asset_paths) == 96,
           "保存清单数量不正确")
    unreal.log(
        "SSLM八槽隔离作者已保存：DataTable 96行，DataAsset 96份；"
        "新建=" + str(created_count) + "，按目录更新=" + str(updated_count) +
        "；正式英雄默认技能集未写入。"
    )
    return {
        "Status": "AuthoringSavedNeedsIndependentColdReload",
        "SavedPaths": saved_paths,
        "HeroAssetMap": contract["heroes"],
        "BalanceTableObjectPath": TABLE_OBJECT_PATH,
        "AbilityDefinitions": 96,
        "BalanceRows": 96,
        "CreatedDefinitions": created_count,
        "UpdatedOwnedDraftDefinitions": updated_count,
        "BalanceTableCreated": table_created,
        "HeroDefaultAbilitySetIdsChanged": 0,
        "NativeDataValidationVerified": False,
        "IndependentColdReloadVerified": False,
        "NativeGASRuntimeVerified": False,
        "VFXAssetsVerified": False,
        "AnimationAssetsVerified": False,
        "BlueprintEightSlotsVerified": False,
    }


# Monolith调用器只能显式执行author()；import脚本或仅打开编辑器均不会创建资产。
