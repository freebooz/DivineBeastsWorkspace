#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
十二生肖技能策划候选清单校验（不创建生产技能）。
职责：核对60个候选与真实图源的生肖/槽位一一对应，同时保证未批准的技能编号和权威数值为空。
作用域：工程静态证据；不取代UE5.8编辑器的UAsset/DataTable/GameplayAbility验收。
使用：python Tests/Assets/ValidateZodiacAbilityDesignCandidates.py
退出码：0表示策划候选结构与图源清单一致；1表示存在重复、越界或未经批准的生产值。
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DESIGN = ROOT / "Docs/Implementation/ZodiacAbilityDesignCandidates.json"
SOURCE = ROOT / "Docs/Art/ZodiacSkillIconPrompts.json"
HEROES = {
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar",
}
SLOTS = {"BasicAttack", "Passive", "Active01", "Active02", "Ultimate"}
EXPECTED_TAGS = {
    "BasicAttack": "Platform.Ability.Input.DivineBeasts.Primary",
    "Passive": None,
    "Active01": "Platform.Ability.Input.DivineBeasts.Slot1",
    "Active02": "Platform.Ability.Input.DivineBeasts.Slot2",
    "Ultimate": "Platform.Ability.Input.DivineBeasts.Slot4",
}


def main() -> int:
    """逐项检查逻辑身份和候选内容，不创建任何引擎资源。"""
    errors: list[str] = []
    candidates = json.loads(DESIGN.read_text(encoding="utf-8"))
    icons = json.loads(SOURCE.read_text(encoding="utf-8"))
    if candidates.get("Status") != "DesignCandidatesNotProductionApproved":
        errors.append("策划候选不得错误标记成生产批准状态")
    hero_keys: set[str] = set()
    all_slots: set[tuple[str, str]] = set()
    for hero in candidates.get("Heroes", []):
        hero_id = hero.get("HeroDefinitionId", "")
        short = hero_id.removeprefix("Hero.Zodiac.")
        if short not in HEROES or not hero_id.startswith("Hero.Zodiac."):
            errors.append(f"无效英雄身份 {hero_id}")
        if short in hero_keys:
            errors.append(f"重复英雄身份 {short}")
        hero_keys.add(short)
        if hero.get("Approval") != "Pending":
            errors.append(f"{short} 未获审批不得标成已完成")
        skill_slots: set[str] = set()
        for skill in hero.get("Skills", []):
            slot = skill.get("Slot", "")
            if slot not in SLOTS or slot in skill_slots:
                errors.append(f"{short} 无效或重复槽位 {slot}")
            skill_slots.add(slot)
            all_slots.add((short, slot))
            if not skill.get("ProposedNameZh", "").strip():
                errors.append(f"{short}/{slot} 中文候选技能名称为空")
            if skill.get("InputTagCandidate") != EXPECTED_TAGS.get(slot):
                errors.append(f"{short}/{slot} 输入标签与现有GAS路由不一致")
            for required_none in ("AbilityId", "BaseDamage", "MomentumCost", "CooldownSeconds"):
                if skill.get(required_none) is not None:
                    errors.append(f"{short}/{slot} 的 {required_none} 未经批准不得填写生产值")
            if skill.get("EngineAssetVerified") is not False:
                errors.append(f"{short}/{slot} 不能伪称已完成UE资产验收")
        if skill_slots != SLOTS:
            errors.append(f"{short} 候选槽位不足或额外增加")
    source_keys = {(icon["Hero"], icon["Slot"]) for icon in icons.get("Icons", [])}
    if hero_keys != HEROES or all_slots != source_keys or len(all_slots) != 60:
        errors.append("12生肖×5候选技能与60图源一一对应失败")
    if len(source_keys) != len(icons.get("Icons", [])):
        errors.append("技能图源已有重复英雄+槽位")
    for issue in errors:
        print("FAIL | " + issue)
    if errors:
        print(f"RESULT | FAILED Count={len(errors)}")
        return 1
    print("RESULT | PASS Heroes=12 SkillCandidates=60 ExistingArtworkSlots=60")
    print("SLOT_POLICY | Primary+Passive+Slot1+Slot2+Slot4；Slot3留作扩展")
    print("PRODUCTION | FormalAbilityIds=0 DamageValues=0 UEAssetVerified=0")
    print("LIMITS | 策划候选不是已批准的正式技能，也不是UE编辑器内生成的资产。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
