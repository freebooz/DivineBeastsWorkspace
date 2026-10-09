# -*- coding: utf-8 -*-
"""《神兽联盟》GAS 精简属性集静态门禁（不依赖编辑器运行）。

范围：只读审查三层归属、GAS AttributeSet字段个数、复制条件及元属性不复制。
现行基线：2个具体属性集、6个可复制状态字段＋2个非复制伤害/治疗元属性；
生命/上限2项公开，增伤/减伤与气势4项OwnerOnly（仅拥有者）。
攻击/防御归集为DamageBonus和DamageReduction，护盾用有限时GAS效果实例消耗容量。
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
        "fields": {"Health", "MaxHealth", "DamageBonus", "DamageReduction", "IncomingDamage", "IncomingHealing"},
        "meta": {"IncomingDamage", "IncomingHealing"},
        "public": {"Health", "MaxHealth"},
        "owner": {"DamageBonus", "DamageReduction"},
        "role_zh": "第一层：生命、统一加减伤与两项不复制的瞬时结算",
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
# 用户已取消的属性不得重新成为GAS数值：属性层与表现层不要另立同义字段。
REMOVED = {
    "AttackSpeed", "Poise", "MaxPoise", "PoiseRegen",
    "MomentumGainMultiplier", "MomentumDecayRate", "Tenacity",
    "Armor", "MagicResistance", "ArmorPenetration", "MagicPenetration",
    "AttackPower", "AbilityPower", "Penetration", "Resistance",
    "CriticalChance", "CriticalDamage", "Shield", "MaxShield",
}
issues = []
found = set()
public_count = owner_count = meta_count = 0

def expected_condition(name, cpp):
    """解析注册宏；复制条件的类/字段必须和公开头反射字段一一对应。"""
    result = {}
    # 唯一平台Combat与项目Momentum均使用显式DOREPLIFETIME，检查复制权限而非宣称带宽数值。
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
    issues.append("已取消的GAS数值仍被反射：" + str(REMOVED & found))
# 已退役的攻击/防御类和盾数值类不应留空壳；真正效果在既有Combat和GAS内。
for retired in ("GamePlatformOffenseAttributeSet", "GamePlatformDefenseAttributeSet"):
    for scope in ("Public", "Private"):
        if (COMBAT / scope / "Attributes" / (retired + (".h" if scope == "Public" else ".cpp"))).exists():
            issues.append("未删除同义攻击/防御属性集：" + retired)
# 移除最后一个控制数值后不留下空反射类；控制标签和GameplayEffect由Combat继续提供。
control_class = COMBAT / "Public/Attributes/GamePlatformControlAttributeSet.h"
control_impl = COMBAT / "Private/Attributes/GamePlatformControlAttributeSet.cpp"
if control_class.exists() or control_impl.exists():
    issues.append("用户已取消控制韧性设计，不得保留无职责的控制属性集类")
combat_impl = (COMBAT / "Private/Components/GamePlatformCombatComponent.cpp").read_text(encoding="utf-8-sig")
math_header = (COMBAT / "Public/Types/GamePlatformCombatMath.h").read_text(encoding="utf-8-sig")
math_impl = (COMBAT / "Private/Damage/GamePlatformCombatMath.cpp").read_text(encoding="utf-8-sig")
for rejected in ("GetTenacityAttribute", "GetTenacity()", "CalculateControlDuration",
                 "GetSet<UGamePlatformControlAttributeSet>", "AddSet<UGamePlatformControlAttributeSet>"):
    if rejected in combat_impl + math_header + math_impl:
        issues.append("服务器战斗主链仍包含已废止的控制韧性调用：" + rejected)
if "SetDuration(DurationSeconds, true)" not in combat_impl:
    issues.append("控制效果必须直接使用服务器验证后的技能持续时间")
if "Result.FinalMagnitude = DurationSeconds;" not in combat_impl:
    issues.append("控制结果应记录实际应用的原始技能持续时间")
# 防止删除韧性时误删控制功能：平台仍负责Stun/Silence的类型判定、GE施放和时长边界。
required_control = (
    "FMath::IsFinite(DurationSeconds)",
    "DurationSeconds > Settings->MaxControlDuration",
    "EGamePlatformControlType::Stun",
    "EGamePlatformControlType::Silence",
    "UGamePlatformStunGameplayEffect::StaticClass()",
    "UGamePlatformSilenceGameplayEffect::StaticClass()",
    "ApplyGameplayEffectSpecToTarget",
    "PublishCombatEvent",
)
for required in required_control:
    if required not in combat_impl:
        issues.append("取消韧性不应删除服务器控制技能原有功能：" + required)
# 历史结果身份保留以免枚举序列化值漂移，但不能继续在当前控制流程发出虚构抵抗。
if "Result.ResultTags.AddTag(GamePlatformCombatTags::Result_ControlResisted)" in combat_impl:
    issues.append("用户取消韧性后不可再根据数值产生控制抵抗结果")
# 增减伤统一走GAS聚合后的当前值；物理/法术仅保留已发布类型身份。
execution = (COMBAT / "Private/Execution/GamePlatformDamageExecutionCalculation.cpp").read_text(encoding="utf-8-sig")
spec_header = (COMBAT / "Public/Types/GamePlatformCombatSpec.h").read_text(encoding="utf-8-sig")
result_header = (COMBAT / "Public/Types/GamePlatformCombatResult.h").read_text(encoding="utf-8-sig")
balance_header = (PLUGINS / "DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/"
                  "Public/Types/DivineBeastsAbilityBalanceRow.h").read_text(encoding="utf-8-sig")
ability_impl = (PLUGINS / "DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/"
                "Private/Abilities/DivineBeastsConfiguredGameplayAbility.cpp").read_text(encoding="utf-8-sig")
for token in ("Formula.DamageBonus = Source->GetDamageBonus()",
              "Formula.DamageReduction = Target->GetDamageReduction()"):
    if token not in execution:
        issues.append("伤害执行缺少GAS聚合后的统一加减伤：" + token)
for token in ("Input.DamageBonus", "Input.DamageReduction",
              "Input.DamageType == EGamePlatformDamageType::TrueDamage"):
    if token not in math_impl:
        issues.append("平台纯加减伤计算缺少必要入口：" + token)
for token in ("GetAvailableShieldEffectCapacity()", "ConsumeShieldEffectCapacity",
              "ApplyShield(", "ClearShieldEffects()"):
    if token not in combat_impl:
        issues.append("删除盾属性后必须保留基于GE实例的盾吸收和清理：" + token)
shield_ge = (COMBAT / "Private/Effects/GamePlatformCombatGameplayEffects.cpp").read_text(encoding="utf-8-sig")
if "UGamePlatformShieldGameplayEffect::UGamePlatformShieldGameplayEffect" not in shield_ge:
    issues.append("缺少限时护盾GameplayEffect，不能用空值冒充盾机制")
for token in ("GetCriticalChance()", "GetCriticalDamage()", "MakeDeterministicUnitRoll",
              "IsCriticalHit(", "bWasCritical", "bCanCritical", "CriticalRoll"):
    if any(token in body for body in (combat_impl, math_header, math_impl, execution,
                                      spec_header, result_header, balance_header, ability_impl)):
        issues.append("已取消暴击机制不得保留运行入口：" + token)
moba = PLUGINS / "MobaCommon/Presentation/MobaPresentation/Source"
moba_adapter = (moba / "MobaPresentationClient/Private/Adapters/MobaPresentationFactAdapters.cpp").read_text(encoding="utf-8-sig")
moba_registry = (moba / "MobaPresentationRuntime/Private/MobaPresentationSemanticRegistry.cpp").read_text(encoding="utf-8-sig")
moba_types = (moba / "MobaPresentationRuntime/Public/Types/MobaPresentationTypes.h").read_text(encoding="utf-8-sig")
moba_policy = (moba / "MobaPresentationRuntime/Private/Feedback/MobaHitFeedbackPolicy.cpp").read_text(encoding="utf-8-sig")
moba_client = (moba / "MobaPresentationClient/Private/MobaPresentationClientSubsystem.cpp").read_text(encoding="utf-8-sig")
for text_part in (moba_adapter, moba_registry, moba_types, moba_policy, moba_client):
    if any(x in text_part for x in ("bCritical", "FromCriticalFact", "Combat_Critical", "CriticalExtraFrames")):
        issues.append("MOBA表现还存在真实暴击事实生成/消费")
if len(found) != 8 or public_count != 2 or owner_count != 4 or meta_count != 2:
    issues.append("预期8字段/2公开/4拥有者/2元属性，与实际不符")

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
