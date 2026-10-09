# -*- coding: utf-8 -*-
"""《神兽联盟》GAS 精简属性集静态门禁（不依赖编辑器运行）。

范围：只读审查三层归属、GAS AttributeSet字段个数、复制条件及元属性不复制。
预期：5个已存在的具体属性集、16个正式数值属性 + 2个非复制伤害/治疗元属性；
其中生命/护盾4项随Actor网络相关性公开，剩余12项仅OwnerOnly（拥有者）。
用途：防止新代码把没有真实运行消费者的属性重新加入GAS并无差别广播。
限制：源码审查不等于UHT、引擎运行、真实Owner识别、迷雾视野或带宽验收。
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLUGINS = ROOT / "Game/Plugins"
COMBAT = PLUGINS / "GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat"
DBA = PLUGINS / "DivineBeasts/DBAGameplay/Source/DivineBeastsCharactersRuntime"

# 文件、数值字段、复制条件和是否在低层公开。中立GAS基础类没有任何数值字段。
MODEL = {
    "GamePlatformCombatAttributeSet": {
        "module": COMBAT,
        "fields": {"Health", "MaxHealth", "Shield", "MaxShield", "IncomingDamage", "IncomingHealing"},
        "meta": {"IncomingDamage", "IncomingHealing"},
        "public": {"Health", "MaxHealth", "Shield", "MaxShield"},
        "owner": set(),
        "role_zh": "第一层：生命、护盾与伤害/治疗瞬时结算",
    },
    "GamePlatformOffenseAttributeSet": {
        "module": COMBAT,
        "fields": {"AttackPower", "AbilityPower", "CriticalChance", "CriticalDamage", "ArmorPenetration", "MagicPenetration"},
        "meta": set(),
        "public": set(),
        "owner": {"AttackPower", "AbilityPower", "CriticalChance", "CriticalDamage", "ArmorPenetration", "MagicPenetration"},
        "role_zh": "第一层：通用攻击与技能输出（拥有者私有）",
    },
    "GamePlatformDefenseAttributeSet": {
        "module": COMBAT,
        "fields": {"Armor", "MagicResistance", "DamageReduction"},
        "meta": set(),
        "public": set(),
        "owner": {"Armor", "MagicResistance", "DamageReduction"},
        "role_zh": "第一层：通用防御（拥有者私有）",
    },
    "GamePlatformControlAttributeSet": {
        "module": COMBAT,
        "fields": {"Tenacity"},
        "meta": set(),
        "public": set(),
        "owner": {"Tenacity"},
        "role_zh": "第一层：控制时长抗性（拥有者私有）",
    },
    "DivineBeastsMomentumAttributeSet": {
        "module": DBA,
        "fields": {"Momentum", "MaxMomentum"},
        "meta": set(),
        "public": set(),
        "owner": {"Momentum", "MaxMomentum"},
        "role_zh": "第三层：神兽联盟气势（拥有者私有）",
    },
}
REMOVED = {"AttackSpeed", "Poise", "MaxPoise", "PoiseRegen", "MomentumGainMultiplier", "MomentumDecayRate"}
issues = []
found = set()
public_count = owner_count = meta_count = 0

def expected_condition(name, cpp):
    """解析注册宏；复制条件的类/字段必须和公开头反射字段一一对应。"""
    result = {}
    # 展开现有两个明确的源码宏：本脚本不执行预处理器，更不修改引擎头。
    if name == "GamePlatformOffenseAttributeSet":
        macro = re.search(r"#define\s+GP_REP_OFFENSE\(Name\).*?COND_(\w+)", cpp)
        cond = macro.group(1) if macro else None
        for field in re.findall(r"^\s+GP_REP_OFFENSE\((\w+)\);", cpp, re.M):
            result[field] = cond
    else:
        matches = re.findall(
            rf"DOREPLIFETIME_CONDITION_NOTIFY\(\s*U{name}\s*,\s*(\w+)\s*,\s*COND_(\w+)",
            cpp,
        )
        result = {field: cond for field, cond in matches}
    return result

for cls, cfg in MODEL.items():
    hdr = cfg["module"] / "Public/Attributes" / (cls + ".h")
    src = cfg["module"] / "Private/Attributes" / (cls + ".cpp")
    if not hdr.is_file() or not src.is_file():
        issues.append("现有属性集路径缺失：" + cls)
        continue
    ht, ct = (p.read_text(encoding="utf-8-sig") for p in (hdr, src))
    fields = set(re.findall(r"\bFGameplayAttributeData\s+(\w+)\s*;", ht))
    found |= fields
    if fields != cfg["fields"]:
        issues.append(cls + "字段不符，缺失=" + str(cfg["fields"] - fields) +
                      "，多出=" + str(fields - cfg["fields"]))
    if not re.search(r":\s*public\s+UGamePlatformAttributeSet", ht):
        issues.append(cls + "必须单向继承平台中立属性集基类")
    regs = expected_condition(cls, ct)
    expected = {field: "None" for field in cfg["public"]}
    expected.update({field: "OwnerOnly" for field in cfg["owner"]})
    if regs != expected:
        issues.append(cls + "复制策略与归属不符，实际=" + repr(regs))
    if "REPNOTIFY_Always" not in ct or any(
        "ReplicatedUsing=OnRep_" + field not in ht for field in expected
    ):
        issues.append(cls + "缺少GAS复制通知契约")
    if any("ReplicatedUsing=OnRep_" + field in ht for field in cfg["meta"]):
        issues.append(cls + "包含不应复制的瞬时结算Meta")
    public_count += len(cfg["public"])
    owner_count += len(cfg["owner"])
    meta_count += len(cfg["meta"])

# 最后单独复查无数值的中立基类与角色新装配仍只用一个ASC。
base = (PLUGINS / "GamePlatform/Gameplay/GamePlatformAbilitySystem/"
        "Source/GamePlatformAbilitySystem/Public/Attributes/GamePlatformAttributeSet.h")
if not base.is_file() or re.search(r"\bFGameplayAttributeData\s+\w+\s*;", base.read_text(encoding="utf-8-sig")):
    issues.append("平台GAS中立基类不得重复定义具体数值")
hero = (PLUGINS / "DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/"
        "Private/Characters/DivineBeastsGameplayCharacter.cpp")
hero_text = hero.read_text(encoding="utf-8-sig")
if hero_text.count("CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>") != 1:
    issues.append("当前项目英雄必须只有一个ASC权威入口")
if REMOVED & found:
    issues.append("旧预留字段仍存在于GAS反射集合：" + str(REMOVED & found))
if len(found) != 18 or public_count != 4 or owner_count != 12 or meta_count != 2:
    issues.append("预期18字段/4公开/12拥有者/2元属性，与实际不符")

# 没有把敌方不可见数据装入第二层MOBA，也没有将项目气势回写到平台层。
if list((PLUGINS / "MobaCommon").rglob("*AttributeSet.h")):
    issues.append("MOBA层不应复制另一个通用Combat或项目专属属性集")

summary = {
    "status": "PASS" if not issues else "FAIL",
    "concreteAttributeSets": len(MODEL),
    "totalGameplayAttributeFields": len(found),
    "replicatedToRelevantObservers": public_count,
    "replicatedToOwnerOnly": owner_count,
    "unreplicatedMetaFields": meta_count,
    "retiredUnconsumedFields": sorted(REMOVED),
    "byDomain": {name: {
        "fields": sorted(cfg["fields"]), "ownership": cfg["role_zh"]
    } for name, cfg in MODEL.items()},
    "issues": issues,
    "limits": "静态门禁不证明独立编辑器重载、两客户端真实复制、网络性能或历史蓝图兼容",
}
print(json.dumps(summary, ensure_ascii=False, indent=2))
raise SystemExit(0 if not issues else 1)
