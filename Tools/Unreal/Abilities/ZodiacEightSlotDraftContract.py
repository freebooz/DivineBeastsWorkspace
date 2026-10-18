# -*- coding: utf-8 -*-
"""SSLM十二生肖八槽技能草案的纯Python契约转换器。

只读取机器可读设计与CSV数值源；不导入unreal，不创建或修改引擎资产。
在Monolith作者脚本与静态门禁之间共享相同的校验和映射。
"""
from __future__ import annotations

import csv
import hashlib
import io
import json
import math
import re
from typing import Any

CATALOG_ID = "SSLM.ZodiacEightSlotSkills.V2"
ASSET_OWNER_ID = "DBAGameplay"
ASSET_ROOT = "/DBAGameplay/Abilities/EightSlotDrafts"
TABLE_PACKAGE_PATH = ASSET_ROOT + "/DT_DBA_ZodiacEightSlotDraftBalance"
CSV_COLUMNS = (
    "Name", "AbilityId", "Level", "BaseDamage", "DamageType",
    "CooldownSeconds", "MomentumCost", "CastRangeCm", "AreaRadiusCm",
)
EXPECTED_HEROES = {
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar",
}
SLOT_ORDER = ("Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7")
ROLE_ORDER = ("basic", "attack_a", "attack_b", "ultimate", "mobility", "defense", "control", "signature")
HEALER_CODES = {"rabbit", "goat", "rooster", "dog"}
DAMAGE_TYPES = {"Physical", "Magic", "Untyped"}
ACTION_SHAPES = {
    "damage": {"kind", "baseMagnitude", "hitCount", "type"},
    "movement": {"kind", "distanceCm", "serverNavValidation"},
    "shield": {"kind", "maxHealthFraction", "durationSeconds"},
    "healing": {"kind", "maxHealthFraction", "allowRevive"},
    "control": {"kind", "controlType", "durationSeconds", "slowFraction"},
    "buff": {"kind", "buffType", "durationSeconds", "magnitudeFraction"},
}
ACTION_MAGNITUDES = {
    "damage": ("baseMagnitude", "damage_points"),
    "movement": ("distanceCm", "distance_cm"),
    "shield": ("maxHealthFraction", "max_health_fraction"),
    "healing": ("maxHealthFraction", "max_health_fraction"),
    "control": ("slowFraction", "slow_fraction"),
    "buff": ("magnitudeFraction", "magnitude_fraction"),
}
EXPECTED_INPUTS = (
    ("Primary", 1, "basic", "Platform.Ability.Input.DivineBeasts.Primary", "DBA.PrimaryAbility", "1"),
    ("Slot1", 2, "attack_a", "Platform.Ability.Input.DivineBeasts.Slot1", "DBA.AbilityOne", "2"),
    ("Slot2", 3, "attack_b", "Platform.Ability.Input.DivineBeasts.Slot2", "DBA.AbilityTwo", "3"),
    ("Slot4", 4, "ultimate", "Platform.Ability.Input.DivineBeasts.Slot4", "DBA.UltimateAbility", "4"),
    ("Slot3", 5, "mobility", "Platform.Ability.Input.DivineBeasts.Slot3", "DBA.AbilitySlot3", "5"),
    ("Slot5", 6, "defense", "Platform.Ability.Input.DivineBeasts.Slot5", "DBA.AbilitySlot5", "6"),
    ("Slot6", 7, "control", "Platform.Ability.Input.DivineBeasts.Slot6", "DBA.AbilitySlot6", "7"),
    ("Slot7", 8, "signature", "Platform.Ability.Input.DivineBeasts.Slot7", "DBA.AbilitySlot7", "8"),
)
EXPECTED_INPUT_BY_SLOT = {
    row[0]: {
        "slot": row[0], "displayOrder": row[1], "role": row[2],
        "tag": row[3], "action": row[4], "keyboard": row[5],
    }
    for row in EXPECTED_INPUTS
}


class ContractError(ValueError):
    """设计源不能安全映射为可审阅的数据资产。"""


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def asset_object_path(package_path: str) -> str:
    """隔离目录的包路径转换为UE返回的完整对象路径；不得将包名直接与GetPathName比较。"""
    _require(package_path.startswith(ASSET_ROOT + "/") and
             re.fullmatch(r"/[A-Za-z0-9_/]+", package_path) is not None,
             "仅允许当前隔离目录中的ASCII资产包路径")
    return package_path + "." + package_path.rsplit("/", 1)[-1]


TABLE_OBJECT_PATH = asset_object_path(TABLE_PACKAGE_PATH)


def _finite_number(value: Any, field: str, identity: str) -> float:
    _require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value),
             f"{identity}:{field}必须是有限数值")
    return float(value)


def _validate_zh_text(value: Any, field: str, identity: str, minimum: int, maximum: int) -> str:
    """保留原文而非静默修剪；拒绝空白、纯英文占位及不合理长度的策划字段。"""
    _require(isinstance(value, str), f"{identity}:{field}必须为中文文本")
    _require(value == value.strip() and minimum <= len(value) <= maximum and
             re.search(r"[\u3400-\u9fff]", value) is not None and "\x00" not in value,
             f"{identity}:{field}缺少中文内容、含空白填充或长度越界")
    _require(re.search(r"\b(?:TODO|TBD)\b|待补充|待填写|占位文本", value, re.I) is None,
             f"{identity}:{field}不能以待填占位内容冒充策划设计")
    return value


def _validate_action(action: dict[str, Any], identity: str) -> dict[str, Any]:
    _require(isinstance(action, dict), f"{identity}:行为必须是对象")
    kind = action.get("kind")
    _require(kind in ACTION_SHAPES, f"{identity}:未知行为类型{kind!r}")
    _require(set(action) == ACTION_SHAPES[kind],
             f"{identity}:{kind}行为字段缺失或含未知字段，需先扩展转换契约")
    magnitude_key, magnitude_semantic = ACTION_MAGNITUDES[kind]
    magnitude = _finite_number(action[magnitude_key], magnitude_key, identity)
    duration = _finite_number(action.get("durationSeconds", 0.0), "durationSeconds", identity)
    _require(magnitude >= 0.0 and 0.0 <= duration <= 120.0,
             f"{identity}:{kind}行为幅值或持续时间越界")
    hit_count = 1
    modifier = ""
    damage_type = ""
    nav_validation = False
    allow_revive = False

    if kind == "damage":
        hit_count = action["hitCount"]
        _require(isinstance(hit_count, int) and not isinstance(hit_count, bool) and 1 <= hit_count <= 8,
                 f"{identity}:hitCount必须为1～8的整数")
        damage_type = action["type"]
        _require(damage_type in DAMAGE_TYPES, f"{identity}:行为伤害类型未知")
    elif kind == "movement":
        nav_validation = action["serverNavValidation"]
        _require(nav_validation is True, f"{identity}:移动行为必须声明服务器导航校验")
    elif kind == "shield":
        _require(0.0 < magnitude <= 1.0 and duration > 0.0,
                 f"{identity}:护盾比例或持续时间非法")
    elif kind == "healing":
        allow_revive = action["allowRevive"]
        _require(isinstance(allow_revive, bool) and allow_revive is False,
                 f"{identity}:当前草案不得隐式授予复活能力")
        _require(0.0 < magnitude <= 1.0, f"{identity}:治疗比例非法")
    elif kind == "control":
        modifier = action["controlType"]
        _require(modifier in {"slow", "stun", "silence"}, f"{identity}:控制类型未登记")
        _require(0.0 <= magnitude <= 1.0 and duration > 0.0,
                 f"{identity}:控制比例或持续时间非法")
    elif kind == "buff":
        modifier = action["buffType"]
        _require(isinstance(modifier, str) and re.fullmatch(r"[a-z][a-z0-9_]*", modifier) is not None,
                 f"{identity}:增益类型必须使用稳定小写身份")
        _require(0.0 < magnitude <= 1.0 and duration > 0.0,
                 f"{identity}:增益比例或持续时间非法")

    source_payload = json.dumps(action, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return {
        "ActionKindId": kind,
        "ModifierId": modifier,
        "MagnitudeSemanticId": magnitude_semantic,
        "DamageTypeId": damage_type,
        "Magnitude": magnitude,
        "DurationSeconds": duration,
        "HitCount": hit_count,
        "bServerNavValidation": nav_validation,
        "bAllowRevive": allow_revive,
        "SourcePayloadJson": source_payload,
    }


def _parse_balance_rows(balance_csv: str) -> dict[str, dict[str, Any]]:
    reader = csv.DictReader(io.StringIO(balance_csv, newline=""))
    _require(tuple(reader.fieldnames or ()) == CSV_COLUMNS, "96行数值CSV表头与UE行结构不一致")
    rows: dict[str, dict[str, Any]] = {}
    row_names: set[str] = set()
    for source_row in reader:
        ability_id = source_row["AbilityId"].strip()
        name = source_row["Name"].strip()
        _require(bool(ability_id) and bool(name), "数值CSV含空技能身份或行名")
        _require(ability_id not in rows, f"数值CSV重复AbilityId：{ability_id}")
        _require(name not in row_names, f"数值CSV重复行名：{name}")
        row_names.add(name)
        try:
            row = {
                "Name": name,
                "AbilityId": ability_id,
                "Level": int(source_row["Level"]),
                "BaseDamage": float(source_row["BaseDamage"]),
                "DamageType": source_row["DamageType"].strip(),
                "CooldownSeconds": float(source_row["CooldownSeconds"]),
                "MomentumCost": float(source_row["MomentumCost"]),
                "CastRangeCm": float(source_row["CastRangeCm"]),
                "AreaRadiusCm": float(source_row["AreaRadiusCm"]),
            }
        except (TypeError, ValueError) as error:
            raise ContractError(f"{ability_id}:数值CSV字段格式非法") from error
        for field in ("BaseDamage", "CooldownSeconds", "MomentumCost", "CastRangeCm", "AreaRadiusCm"):
            _require(math.isfinite(row[field]) and row[field] >= 0.0, f"{ability_id}:{field}非法")
        _require(row["Level"] == 1, f"{ability_id}:设计稿只允许生成1级候选数值行")
        _require(row["DamageType"] in DAMAGE_TYPES, f"{ability_id}:CSV伤害类型未知")
        _require(row["CooldownSeconds"] <= 120.0 and row["MomentumCost"] <= 100.0 and
                 row["CastRangeCm"] <= 1000000.0 and row["AreaRadiusCm"] <= 1000000.0,
                 f"{ability_id}:CSV数值超过当前项目边界")
        rows[ability_id] = row
    _require(len(rows) == 96, f"数值CSV必须恰有96行，实际{len(rows)}行")
    return rows


def build_contract(catalog: dict[str, Any], balance_csv: str) -> dict[str, Any]:
    """校验全量设计源并返回作者脚本可直接写入的行、行为及稳定路径映射。"""
    _require(catalog.get("schemaVersion") == 2, "技能设计Schema版本必须为2")
    _require(catalog.get("projectCode") == "SSLM", "项目代码不匹配")
    _require(catalog.get("designerApprovalStatus") == "PENDING_HUMAN_BALANCE_REVIEW",
             "策划审批状态已变化；拒绝使用草案作者")
    _require(catalog.get("nativeUEAssetStatus") == "NOT_AUTHORED",
             "原生资产状态已变化；拒绝覆盖来源状态不明的资源")
    _require(catalog.get("balanceStatus") == "LEVEL1_CANDIDATE_NOT_BALANCED",
             "一级数值状态必须保持未平衡候选，拒绝错误审批状态")
    text_policy = catalog.get("textDesignPolicy", {})
    _require(text_policy.get("language") == "zh-CN" and
             text_policy.get("reviewStatus") == "PENDING_HUMAN_TEXT_AND_VISUAL_REVIEW",
             "中文文本与效果概念设计必须保留待人工审核状态")
    constraints = catalog.get("constraints", {})
    _require(constraints.get("sourceRole") == "DESIGN_ONLY" and
             constraints.get("allowUnimplementedAbilityGrant") is False,
             "未实现技能的服务器授权必须保持禁用")
    _require(catalog.get("requiresNativeUEEditor") is True, "原生资产必须通过UE编辑器制作")
    slot_policy = catalog.get("slotPolicy", {})
    _require(slot_policy.get("maxActiveSlots") == 8 and slot_policy.get("minActiveSlots") == 8 and
             slot_policy.get("innatePassiveOccupiesSlot") is False, "八槽策略字段不符")
    _require(slot_policy.get("legacyUltimatePreservedAtSlot4") is True, "终极技槽位不能离开Slot4")
    input_rows = slot_policy.get("inputSemantics", [])
    _require(len(input_rows) == 8, "技能输入语义必须恰有八项")
    _require(input_rows == [EXPECTED_INPUT_BY_SLOT[s] for s in SLOT_ORDER],
             "输入槽、展示顺序、标签、输入动作或默认键盘绑定发生漂移")
    _require(catalog.get("heroCount") == 12 and catalog.get("skillCount") == 96,
             "目录计数不是12位英雄和96项技能")
    heroes = catalog.get("heroes", [])
    _require(len(heroes) == 12, "生肖英雄条目必须恰有12位")
    _require({h.get("heroId", "").rsplit(".", 1)[-1] for h in heroes} == EXPECTED_HEROES,
             "十二生肖英雄目录不完整")

    balance_by_id = _parse_balance_rows(balance_csv)
    seen_ids: set[str] = set()
    seen_asset_paths: set[str] = set()
    skills: list[dict[str, Any]] = []
    heroes_manifest: list[dict[str, Any]] = []
    healer_codes: set[str] = set()

    for hero in heroes:
        hero_id = hero.get("heroId", "")
        hero_name = hero.get("heroNameZh", "")
        hero_code = hero.get("heroCode", "")
        hero_key = hero_id.rsplit(".", 1)[-1]
        _require(hero_code == hero_key.lower(), f"{hero_id}:英雄代码与稳定身份不匹配")
        _require(hero.get("proposedAbilitySetId") == f"dba.abilityset.{hero_code}_eight_draft@1",
                 f"{hero_id}:草案集合身份不匹配")
        trait = hero.get("innateTrait", {})
        _require(trait.get("slotOccupancy") == 0 and trait.get("implementationStatus") == "design_only" and
                 trait.get("notGrantedByDefault") is True, f"{hero_id}:固有被动不得占用主动槽或默认授权")
        entries = hero.get("slots", [])
        _require(len(entries) == 8, f"{hero_name}:必须有8项主动技能")
        _require([entry.get("slotId") for entry in entries] == list(SLOT_ORDER),
                 f"{hero_name}:技能槽顺序不符合统一语义")
        _require([entry.get("role") for entry in entries] == list(ROLE_ORDER),
                 f"{hero_name}:技能角色分类顺序不完整")
        if any(action.get("kind") == "healing" for entry in entries for action in entry.get("actions", [])):
            healer_codes.add(hero_code)
        hero_manifest = {
            "heroId": hero_id,
            "heroNameZh": hero_name,
            "proposedAbilitySetIdNotApplied": hero["proposedAbilitySetId"],
            "assetFolder": ASSET_ROOT,
            "skills": [],
        }

        for entry in entries:
            identity = entry.get("abilityId", "")
            _require(re.fullmatch(r"dba\.ability\.[a-z0-9_]+@1", identity) is not None,
                     f"{identity}:技能逻辑ID格式非法")
            _require(identity.startswith(f"dba.ability.{hero_code}_eight_"),
                     f"{identity}:技能身份跨越英雄命名空间")
            _require(identity not in seen_ids, f"{identity}:重复技能逻辑身份")
            seen_ids.add(identity)
            _require(entry.get("level") == 1, f"{identity}:仅允许Level 1")
            _require(entry.get("implementationStatus") == "DESIGN_ONLY_UNIMPLEMENTED" and
                     entry.get("approvedForLiveGAS") is False, f"{identity}:草案不能获得正式GAS授权")
            _require(entry.get("energyCost") == 0 and 0 <= entry.get("momentumCost", -1) <= 100,
                     f"{identity}:能量费用未定或气势费用越界")
            _require(entry.get("damageType") in DAMAGE_TYPES and
                     (entry["damageType"] != "Magic" or entry["momentumCost"] == 0),
                     f"{identity}:伤害类型或法术气势约束非法")
            _require(entry.get("vfx", {}).get("heroProfileId") ==
                     f"Presentation.VFX.Hero.Zodiac.{hero_key}.Default",
                     f"{identity}:生肖视觉配置身份错配")
            _require(entry.get("vfx", {}).get("assetPath") is None and
                     entry.get("vfx", {}).get("assetStatus") == "NOT_AUTHORED" and
                     entry.get("animation", {}).get("montagePath") is None and
                     entry.get("animation", {}).get("assetStatus") == "NOT_AUTHORED" and
                     entry.get("icon", {}).get("assetPath") is None and
                     entry.get("icon", {}).get("assetStatus") == "NOT_BOUND",
                     f"{identity}:未制作表现资源不能伪造硬引用或资产状态")
            _require(bool(entry.get("nameZh")) and bool(entry.get("descriptionZh")) and
                     bool(entry.get("intentTarget")) and bool(entry.get("actions")),
                     f"{identity}:策划中文说明、目标或行为缺失")
            # 名称/说明用于玩家界面，提示词用于编辑器审核；三者均完整进入契约与源摘要。
            draft_text = {
                "DraftDisplayNameZh": _validate_zh_text(entry.get("nameZh"), "nameZh", identity, 2, 48),
                "DraftDescriptionZh": _validate_zh_text(entry.get("descriptionZh"), "descriptionZh", identity, 8, 1200),
                "DraftDesignPromptZh": _validate_zh_text(entry.get("designPromptZh"), "designPromptZh", identity, 120, 3000),
            }
            _require(hero_name in draft_text["DraftDesignPromptZh"] and
                     draft_text["DraftDisplayNameZh"] in draft_text["DraftDesignPromptZh"],
                     f"{identity}:提示词未对应当前英雄及技能中文名称")
            if entry.get("abilityKey") in {"battle_fervor", "trickster_stride", "rage_focus"}:
                _require(entry["intentTarget"] == "self",
                         f"{identity}:自身特色增益不得错误要求选择敌方目标")
            _require(len(entry["actions"]) <= 8, f"{identity}:行为数量超过原生设计容量")
            mapped_actions = [_validate_action(action, identity) for action in entry["actions"]]
            damage_actions = [a for a in mapped_actions if a["ActionKindId"] == "damage"]
            has_healing = any(a["ActionKindId"] == "healing" for a in mapped_actions)
            _require(all(a["DamageTypeId"] == entry["damageType"] and
                         a["Magnitude"] == entry["baseDamage"] and
                         a["HitCount"] == entry["hitCount"] for a in damage_actions),
                     f"{identity}:伤害行为的类型、每段数值或段数与一级源不一致")
            _require(not (damage_actions and has_healing) or entry["intentTarget"] == "area_center_mixed",
                     f"{identity}:混合伤害治疗必须声明同一范围中心，分别结算敌我")
            _require(entry.get("role") != "mobility" or any(a["ActionKindId"] == "movement" for a in mapped_actions),
                     f"{identity}:移动槽没有服务器校验位移")
            _require(entry.get("role") != "defense" or any(a["ActionKindId"] == "shield" for a in mapped_actions),
                     f"{identity}:防御槽没有护盾行为")
            _require(entry.get("role") != "control" or any(a["ActionKindId"] == "control" for a in mapped_actions),
                     f"{identity}:控制槽没有控制行为")
            if entry.get("role") == "basic":
                _require(entry.get("baseDamage", 0) > 0, f"{identity}:基础攻击必须有候选伤害")

            row_name = identity.rsplit(".", 1)[-1].removesuffix("@1") + "_L1"
            csv_row = balance_by_id.get(identity)
            _require(csv_row is not None, f"{identity}:CSV数值行缺失")
            _require(csv_row["Name"] == row_name, f"{identity}:CSV行名不符合稳定命名")
            mirror = {
                "BaseDamage": entry["baseDamage"],
                "DamageType": entry["damageType"],
                "CooldownSeconds": entry["cooldownSeconds"],
                "MomentumCost": entry["momentumCost"],
                "CastRangeCm": entry["castRangeCm"],
                "AreaRadiusCm": entry["areaRadiusCm"],
            }
            for field, expected in mirror.items():
                if field == "DamageType":
                    _require(csv_row[field] == expected, f"{identity}:{field}与策划源不一致")
                else:
                    expected_number = _finite_number(expected, field, identity)
                    _require(math.isclose(csv_row[field], expected_number, rel_tol=0.0, abs_tol=1e-6),
                             f"{identity}:{field}与策划源不一致")
            _require(csv_row["Level"] == entry["level"], f"{identity}:CSV等级与策划源不一致")
            _require(csv_row["BaseDamage"] == 0.0 or csv_row["DamageType"] != "Untyped",
                     f"{identity}:有伤害的技能不能声明Untyped行类型")

            input_semantics = EXPECTED_INPUT_BY_SLOT[entry["slotId"]]
            _require(entry.get("displayOrder") == input_semantics["displayOrder"] and
                     entry.get("role") == input_semantics["role"],
                     f"{identity}:技能槽位显示顺序或角色与输入契约不一致")
            key = "".join(segment.title() for segment in entry["abilityKey"].split("_"))
            asset_path = ASSET_ROOT + f"/DA_DBA_{hero_key}_Eight_{key}"
            _require(asset_path not in seen_asset_paths, f"{identity}:DataAsset对象名冲突")
            seen_asset_paths.add(asset_path)
            digest_payload = {
                "catalogId": CATALOG_ID,
                "hero": {"heroId": hero_id, "heroNameZh": hero_name},
                "skill": entry,
                "balanceRow": csv_row,
                "input": input_semantics,
            }
            source_hash = hashlib.sha256(json.dumps(
                digest_payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")
            ).encode("utf-8")).hexdigest()
            planned = {
                "slotId": entry["slotId"],
                "displayOrder": entry["displayOrder"],
                "roleId": entry["role"],
                "roleZh": entry["roleZh"],
                "abilityId": identity,
                "nameZh": draft_text["DraftDisplayNameZh"],
                "descriptionZh": draft_text["DraftDescriptionZh"],
                "designPromptZh": draft_text["DraftDesignPromptZh"],
                "assetPackagePath": asset_path,
                "assetObjectPath": asset_object_path(asset_path),
                "balanceRowName": row_name,
                "inputTag": input_semantics["tag"],
                "inputActionId": input_semantics["action"],
                "defaultKeyboardBinding": input_semantics["keyboard"],
                "sourceSha256": source_hash,
                "draftOnly": True,
                "approvedForLiveGAS": False,
            }
            skills.append({
                "hero": hero,
                "entry": entry,
                "input": input_semantics,
                "asset_path": asset_path,
                "row_name": row_name,
                "balance_row": csv_row,
                "actions": mapped_actions,
                "draft_text": draft_text,
                "source_sha256": source_hash,
            })
            hero_manifest["skills"].append(planned)
        heroes_manifest.append(hero_manifest)

    _require(len(seen_ids) == 96 and len(skills) == 96, "必须恰有96个唯一技能身份")
    _require(set(balance_by_id) == seen_ids, "数值CSV含未知技能行或缺少设计技能行")
    _require(healer_codes == HEALER_CODES, "治疗职责必须只分配给卯兔、未羊、酉鸡和戌狗")

    return {
        "catalogId": CATALOG_ID,
        "assetOwnerId": ASSET_OWNER_ID,
        "assetRoot": ASSET_ROOT,
        "tableObjectPath": TABLE_OBJECT_PATH,
        "tablePackagePath": TABLE_PACKAGE_PATH,
        "balanceRows": [skill["balance_row"] for skill in skills],
        "skills": skills,
        "heroes": heroes_manifest,
        "sourceCatalogStatus": catalog["designerApprovalStatus"],
        "nativeAssetStatus": catalog["nativeUEAssetStatus"],
    }


def build_manifest(catalog: dict[str, Any], balance_csv: str) -> dict[str, Any]:
    contract = build_contract(catalog, balance_csv)
    return {
        "schemaVersion": 2,
        "catalogId": CATALOG_ID,
        "projectCode": "SSLM",
        "sourceCatalog": "Docs/Design/ZodiacEightSlotSkills_V2.json",
        "sourceBalanceCsv": "Game/Plugins/DivineBeasts/DBAGameplay/SourceArt/Abilities/DT_DBA_ZodiacEightSlotDraftBalance_V2.csv",
        "sourceCatalogStatus": contract["sourceCatalogStatus"],
        "designerApprovalStatus": "PENDING_HUMAN_BALANCE_REVIEW",
        "textReviewStatus": "PENDING_HUMAN_TEXT_AND_VISUAL_REVIEW",
        "nativeAssetStatus": "NOT_AUTHORED",
        "formalAbilitySetAssignmentPerformed": False,
        "liveGASAuthorizationPerformed": False,
        "resourceStatus": {
            "nativeDataAssets": "NOT_AUTHORED",
            "nativeBalanceDataTable": "NOT_AUTHORED",
            "vfxAssetsAuthored": False,
            "animationAssetsAuthored": False,
            "iconAssetsBound": False,
        },
        "expectedCounts": {"heroes": 12, "dataAssets": 96, "balanceRows": 96},
        "assetOwnerId": ASSET_OWNER_ID,
        "assetRoot": ASSET_ROOT,
        "balanceTableObjectPath": TABLE_OBJECT_PATH,
        "balanceTablePackagePath": TABLE_PACKAGE_PATH,
        "heroes": contract["heroes"],
    }


def render_text_review(catalog: dict[str, Any], balance_csv: str) -> str:
    """从同一份通过校验的真源生成96项中文审核索引；不创建概念图片或引擎资产。"""
    contract = build_contract(catalog, balance_csv)
    tick = chr(96)
    lines = [
        "# SSLM十二生肖96项技能中文文本与设计提示词审核索引",
        "",
        "> 自动派生：修改技能设计JSON真源后重新生成本文件，禁止维护第二套技能文案。",
        "> 当前为待人工文本、美术与数值审核的设计；概念图片、原生数据资产、动画与特效均未由本文件制作。",
        "",
        "中文名称用于技能标题，中文说明用于角色创建/技能详情等展示；中文设计提示词仅供策划与美术制作审核，不作为玩家说明。",
        "伤害值为一级候选的单次命中基础伤害，多段次数为单目标上限；未审批成长曲线、持续伤害计量基数/跳频和实际执行规则。",
        "全局配色与特效性能分别引用现行V3.0配色规范及十二生肖特效专项规范，不创建新的色彩或性能标准。",
        "",
    ]
    for hero in contract["heroes"]:
        lines.extend([f"## {hero['heroNameZh']}", "",
                      f"英雄身份：{tick}{hero['heroId']}{tick}（稳定英雄编号）；8项主动技能，固有被动不占八槽。", ""])
        for skill in hero["skills"]:
            lines.extend([
                f"### {skill['displayOrder']}. {skill['nameZh']}", "",
                f"- 技能身份：{tick}{skill['abilityId']}{tick}（稳定技能编号）",
                f"- 输入槽位：{tick}{skill['slotId']}{tick}（{skill['roleZh']}，默认键位{skill['defaultKeyboardBinding']}；角色标识{tick}{skill['roleId']}{tick}）",
                f"- 规划资产：{tick}{skill['assetObjectPath']}{tick}（未制作的草案目标路径）",
                f"- 一级数值行：{tick}{skill['balanceRowName']}{tick}（候选数值，待平衡）", "",
                "**中文技能说明**", "", skill["descriptionZh"], "",
                "**中文技能效果概念图设计提示词**", "", skill["designPromptZh"], "",
            ])
    return "\n".join(lines)


if __name__ == "__main__":
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser(description="校验八槽技能源并生成原生资产路径计划")
    parser.add_argument("--catalog", required=True, help="JSON设计源路径")
    parser.add_argument("--balance-csv", required=True, help="96行候选数值CSV路径")
    parser.add_argument("--output", required=True, help="输出明确标注未制作的资产路径计划")
    parser.add_argument("--review-output", help="可选：同步生成96项中文说明及提示词审核Markdown索引")
    args = parser.parse_args()
    catalog_value = json.loads(Path(args.catalog).read_text(encoding="utf-8-sig"))
    csv_value = Path(args.balance_csv).read_text(encoding="utf-8-sig")
    manifest_value = build_manifest(catalog_value, csv_value)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(manifest_value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    if args.review_output:
        review_path = Path(args.review_output)
        review_path.parent.mkdir(parents=True, exist_ok=True)
        review_path.write_text(render_text_review(catalog_value, csv_value), encoding="utf-8")
    print("SSLM八槽契约通过：12位生肖、96项技能身份/中文名称/说明/提示词、96行候选数值、96条隔离资产路径；原生资产仍未制作。")

