#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
《神兽联盟》十二生肖技能资产交付预检。

职责：在不启动虚幻编辑器、不更改资产的前提下，确认技能主数据清单与磁盘文件
      是否完整；将“资产存在/结构正确”和“全部角色已授权可运行”分开判断。
端侧：工程开发工具，非客户端/服务器运行时实现。
用法：
  python Tests/Assets/ValidateZodiacAbilityDelivery.py --inventory
  python Tests/Assets/ValidateZodiacAbilityDelivery.py --release
退出码：0=所选范围预检通过；1=清单/磁盘真实性错误；2=清单真实但正式交付缺项。
限制：release 模式也只是交付前置门禁，不能替代 UE AssetRegistry、C++ 构建、
      UE Automation、双客户端专用服务器联机、真实 Cook/Stage 和人工审核。
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INVENTORY_PATH = ROOT / "Docs/Implementation/ZodiacAbilityAssetInventory.json"
HEROES = (
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar",
)
VISUAL_ROOT = (
    ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
    "DBAUIPack_Core/Content/UI/Combat"
)
HERO_PACK_ROOT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes"
DEFAULT_ID_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+\.[A-Za-z][A-Za-z0-9_]*@[1-9][0-9]*$")


def require_files(inventory: dict) -> list[str]:
    """逐项核对磁盘实体，阻止将JSON规划描述冒充实际非空引擎资产。"""
    defects: list[str] = []
    seen_ids: set[str] = set()
    heroes = inventory.get("Heroes", [])
    if not isinstance(heroes, list) or len(heroes) != 12:
        return ["英雄登记数量必须为12。"]

    for expected, hero in zip(HEROES, heroes):
        if not isinstance(hero, dict):
            defects.append(f"{expected}: 清单记录不是对象。")
            continue
        hero_id = hero.get("HeroDefinitionId")
        if hero_id != f"Hero.Zodiac.{expected}" or hero_id in seen_ids:
            defects.append(f"{expected}: 英雄稳定身份缺失、重复或顺序不匹配。")
        if isinstance(hero_id, str):
            seen_ids.add(hero_id)

        hero_rel = hero.get("HeroDefinitionAssetFile")
        hero_file = ROOT / str(hero_rel)
        if (
            not isinstance(hero_rel, str)
            or not hero_rel.startswith("Game/Plugins/DivineBeasts/DBAGameplay/Content/Definitions/")
            or not hero_file.is_file()
            or hero_file.stat().st_size == 0
        ):
            defects.append(f"{expected}: 缺失有效的现存英雄主数据文件。")

        texture_dir = HERO_PACK_ROOT / f"DBAHeroPack_{expected}/Content/UI/Abilities"
        textures = list(texture_dir.glob(f"T_DBA_{expected}_*.uasset"))
        if len(textures) != 5 or any(path.stat().st_size < 2048 for path in textures):
            defects.append(f"{expected}: 现有五枚非空美术槽位Texture2D文件不完整。")
        if hero.get("ImportedArtSlotTexturesVerified") != 5:
            defects.append(f"{expected}: 图标来源清单与每英雄五枚磁盘文件不一致。")

    for name in ("WBP_DBA_UI_AbilityBar", "WBP_DBA_UI_AbilitySlot"):
        widget = VISUAL_ROOT / f"{name}.uasset"
        if not widget.is_file() or widget.stat().st_size < 4096:
            defects.append(f"{name}: 正式界面蓝图二进制文件不存在或过小。")

    if inventory.get("SchemaVersion") != 1:
        defects.append("清单SchemaVersion不是当前可读的版本1。")
    if inventory.get("ImportedArtSlotTexture2DVerified") != 60:
        defects.append("总图标纹理数必须与十二英雄每个五枚一致。")
    if inventory.get("HeroDefaultAbilitySetEditorReadbackCount") != 12:
        defects.append("需要真实编辑器逐一核对十二份默认技能集合。")
    return defects


def unmet_release_requirements(inventory: dict) -> list[str]:
    """列出尚缺的上线证据；没有生产真实技能不应默认赋予成功。"""
    missing: list[str] = []
    approved_total = 0
    for hero in inventory["Heroes"]:
        hero_id = hero["HeroDefinitionId"]
        ids = hero.get("ApprovedAbilityIds", [])
        names = hero.get("ApprovedSkillNamesZh", [])
        ability_set_id = hero.get("DefaultAbilitySetId")
        if not isinstance(ability_set_id, str) or not DEFAULT_ID_RE.fullmatch(ability_set_id):
            missing.append(f"{hero_id}: 真实DefaultAbilitySetId尚未批准、设置或格式非法。")
        if len(ids) != 5 or len(names) != 5:
            missing.append(f"{hero_id}: 五个正式技能身份及中文技能名称尚未获批准。")
        elif len(set(ids)) != 5:
            missing.append(f"{hero_id}: 正式技能身份存在重复。")
        approved_total += len(ids)
        if hero.get("SkillBalanceRowsVerified", 0) < len(ids) or not ids:
            missing.append(f"{hero_id}: 各级正式技能数值表尚未完成引擎校验。")
        if len(hero.get("SkillIconPathsVerified", [])) < len(ids) or not ids:
            missing.append(f"{hero_id}: 技能图标尚未按真实AbilityId绑定。")
        if not hero.get("AbilitySetGrantedByDedicatedServer", False):
            missing.append(f"{hero_id}: 缺少专用服务器的真实技能授权证据。")

    if not inventory.get("FormalAbilityNamesApproved", False):
        missing.append("没有经策划批准的十二生肖正式技能清单。")
    if inventory.get("FormalAbilityDefinitionsVerified", 0) < approved_total or approved_total < 60:
        missing.append("实际已加载校验的正式AbilityDefinition少于60项。")
    if inventory.get("FormalSkillIconsVerified", 0) < 60:
        missing.append("图标虽已导入引擎，但未按60个正式AbilityId逐一验收。")
    if inventory.get("AbilityUIProfileAssetsVerified", 0) < 12:
        missing.append("还未创建并验证十二英雄正式AbilityUIProfile主资产。")
    if not inventory.get("AbilityBarWidgetRuntimeBindingVerified", False):
        missing.append("技能栏尚未通过真实服务器GAS授权→客户端Widget绑定运行验收。")

    # 下列证据必须由真实引擎/网络执行后填写；仅填布尔值也不能替代原始日志。
    required_evidence = {
        "UEEditorClientServerBuildVerified": "UE编辑器、客户端与服务器构建",
        "GameplayAutomationVerified": "真实UE玩法与UI自动化",
        "TwoClientDedicatedServerVerified": "双客户端与专用服务器联机技能链",
        "ClientCookVerified": "客户端干净Cook/Stage",
        "ServerCookExclusionVerified": "专用服务器纯表现资产剥离审计",
    }
    for field, label in required_evidence.items():
        if not inventory.get(field, False):
            missing.append(f"缺少{label}通过证据（{field}）。")
    return missing


def main() -> int:
    parser = argparse.ArgumentParser(description="神兽联盟技能交付清单的只读预检")
    parser.add_argument(
        "--inventory", action="store_true", help="只验证清单与现存二进制文件真实存在"
    )
    parser.add_argument(
        "--release", action="store_true", help="追加60技能/12英雄/UE与联机上线门禁"
    )
    args = parser.parse_args()
    if args.inventory and args.release:
        parser.error("--inventory与--release不能同时启用")

    try:
        inventory = json.loads(INVENTORY_PATH.read_text(encoding="utf-8"))
        defects = require_files(inventory)
        if defects:
            for line in defects:
                print("文件或清单错误:", line)
            print(f"INVENTORY_VALIDATION=FAILED;Defects={len(defects)}")
            return 1
        print("INVENTORY_VALIDATION=PASSED;HeroDefinitions=12;ArtSlotTextures=60;WidgetBlueprintFiles=2")

        blockers = unmet_release_requirements(inventory)
        print(f"RELEASE_BLOCKERS={len(blockers)}")
        for reason in blockers:
            print("BLOCKED:", reason)
        if args.release or not args.inventory:
            if blockers:
                print("RELEASE_READINESS=BLOCKED;ExitCode=2")
                return 2
            print("RELEASE_PREFLIGHT=PASSED;需要再核对引擎原始运行证据")
            return 0
        print("INVENTORY_ONLY=PASSED;不代表正式技能可玩或允许上线")
        return 0
    except (OSError, ValueError, TypeError, KeyError) as exc:
        print("预检数据不可读取或字段缺失:", str(exc))
        return 1


if __name__ == "__main__":
    sys.exit(main())
