# -*- coding: utf-8 -*-
"""只读UE开发技能审计：保存并重开编辑器后经Monolith调用audit()，不冒充运行、联机或Cook。"""
import json
from pathlib import Path
import unreal

ROOT = "/Game/Development/DivineBeasts/Abilities"
WORKSPACE = Path(__file__).resolve().parents[3]


def audit():
    """用真实资产核对60身份、12集合、依赖、成本/冷却与正式英雄隔离；失败逐项输出中文定位。"""
    entries = json.loads((WORKSPACE / "Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json").read_text(encoding="utf-8-sig"))["Entries"]
    errors, seen_heroes = [], set()

    def check(condition, message):
        if not condition:
            errors.append(message)

    def identifier(value):
        return str(value.get_editor_property("Namespace")) + "." + str(value.get_editor_property("Name")) + "@" + str(value.get_editor_property("LogicalVersion"))

    def static_value(value):
        return float(value.get_editor_property("ScalableFloatMagnitude").get_editor_property("Value"))

    for entry in entries:
        hero = entry["HeroDefinitionId"].split(".")[-1]
        slot = entry["Slot"]
        label = hero + "/" + slot
        tail = "Primary" if hero == "Rat" and slot == "BasicAttack" else slot
        prefix = "_DBA_" + hero + "_Dev" + tail
        definition = unreal.EditorAssetLibrary.load_asset(ROOT + "/DA" + prefix)
        check(isinstance(definition, unreal.DivineBeastsAbilityDefinition), label + ":缺失真实技能定义")
        if definition is not None:
            check(identifier(definition.get_editor_property("LogicalId")) == entry["DevelopmentAbilityId"], label + ":定义ID偏离草案")
            check(str(definition.get_editor_property("HeroDefinitionId")) == entry["HeroDefinitionId"], label + ":定义属于其他英雄")
        set_path = ROOT + "/DA_DBA_" + hero + "_DevAbilitySet"
        ability_set = unreal.EditorAssetLibrary.load_asset(set_path)
        if hero not in seen_heroes:
            seen_heroes.add(hero)
            check(isinstance(ability_set, unreal.GamePlatformAbilitySetDefinition), hero + ":缺失真实集合")
            if ability_set is not None:
                check(ability_set.get_editor_property("bDevelopmentOnly"), hero + ":缺少开发标记")
                check(identifier(ability_set.get_editor_property("LogicalId")) == "dba.abilityset." + hero.lower() + "_dev@1", hero + ":集合ID与运行覆盖配置不一致")
                grants = ability_set.get_editor_property("Abilities")
                required = ability_set.get_editor_property("RequiredDefinitions")
                expected_ids = {item["DevelopmentAbilityId"] for item in entries if item["HeroDefinitionId"] == entry["HeroDefinitionId"]}
                check(len(grants) == 5 and {identifier(item.get_editor_property("AbilityId")) for item in grants} == expected_ids, hero + ":五项真实授权与草案ID不一致")
                check(len(required) == 5 and {str(item.get_editor_property("PrimaryAssetName")) for item in required} == expected_ids, hero + ":五定义依赖闭包缺失")
            formal = unreal.EditorAssetLibrary.load_asset("/DBAGameplay/Definitions/DA_Hero_Zodiac_" + hero)
            check(formal is not None and str(formal.get_editor_property("DefaultAbilitySetId")) == "None", hero + ":正式英雄默认集合被污染")
            profile = unreal.EditorAssetLibrary.load_asset(ROOT + "/UI/Profiles/DA_DBA_" + hero + "_DevUIProfile")
            check(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), hero + ":客户端UI配置未隔离")
            if profile is not None:
                check({str(item.get_editor_property("AbilityId")) for item in profile.get_editor_property("Entries")} == expected_ids, hero + ":UI编号与实际技能不一致")
        ability_path = ROOT + "/GA" + prefix + ("Native" if hero == "Rat" and slot == "BasicAttack" else "")
        cls = unreal.EditorAssetLibrary.load_blueprint_class(ability_path)
        check(cls is not None, label + ":缺失已编译技能类")
        if cls is None:
            continue
        cdo = unreal.get_default_object(cls)
        check(isinstance(cdo, unreal.DivineBeastsDevelopmentGameplayAbility), label + ":没有真实开发执行器")
        check("SERVER_ONLY" in str(cdo.get_editor_property("NetExecutionPolicy")).upper(), label + ":非服务器独占")
        check(str(cdo.get_editor_property("AbilityDefinitionId").get_editor_property("PrimaryAssetName")) == entry["DevelopmentAbilityId"], label + ":CDO绑定ID错误")
        balance = entry["Level1BalanceDraft"]
        active = slot != "Passive" and balance["BaseDamage"] > 0 and balance["CastRangeCm"] > 0
        check(cdo.get_editor_property("bDevelopmentDamageSampleSupported") == active, label + ":客户端能力标记与真实候选数值不一致")
        if ability_set is not None:
            matched = [grant for grant in ability_set.get_editor_property("Abilities") if identifier(grant.get_editor_property("AbilityId")) == entry["DevelopmentAbilityId"]]
            check(len(matched) == 1, label + ":授权ID重复或缺失")
            if len(matched) == 1:
                actual_input = matched[0].get_editor_property("InputTag")
                check(unreal.GameplayTagLibrary.is_gameplay_tag_valid(actual_input) == bool(entry["InputTag"]), label + ":声明输入槽与实际授权不一致")
        cost = cdo.get_editor_property("CostGameplayEffectClass")
        check((cost is not None) == (balance["MomentumCost"] > 0), label + ":成本GE存在性错误")
        if cost is not None:
            cost_cdo = unreal.get_default_object(cost)
            modifiers = cost_cdo.get_editor_property("Modifiers")
            check("INSTANT" in str(cost_cdo.get_editor_property("DurationPolicy")).upper(), label + ":成本不是瞬时效果")
            check(len(modifiers) == 1, label + ":成本不是唯一气势修改")
            if len(modifiers) == 1:
                check("Momentum" in modifiers[0].get_editor_property("Attribute").export_text(), label + ":成本属性不是气势")
                check(abs(static_value(modifiers[0].get_editor_property("ModifierMagnitude")) + balance["MomentumCost"]) < 0.01, label + ":实际扣气势数值错误")
        cooldown = cdo.get_editor_property("CooldownGameplayEffectClass")
        check((cooldown is not None) == (balance["CooldownSeconds"] > 0), label + ":冷却GE存在性错误")
        if cooldown is not None:
            cool_cdo = unreal.get_default_object(cooldown)
            check("HAS_DURATION" in str(cool_cdo.get_editor_property("DurationPolicy")).upper(), label + ":冷却没有有限持续期")
            check(abs(static_value(cool_cdo.get_editor_property("DurationMagnitude")) - balance["CooldownSeconds"]) < 0.01, label + ":冷却秒数错误")
            check(("DivineBeasts.Development.Cooldown." + hero + "." + slot) in cdo.get_editor_property("DevelopmentCooldownTags").export_text(), label + ":缺少独立冷却标签")
    if errors:
        for error in errors:
            print("DBA_DEVELOPMENT_AUDIT_ERROR=" + error)
        raise RuntimeError("开发技能集成审计失败: " + str(len(errors)))
    print("DBA_DEVELOPMENT_INTEGRATION_AUDIT=PASSED; DEFINITIONS=60; SETS=12; FORMAL_HERO_UNMODIFIED=12; RUNTIME_COOK_UNVERIFIED")
    return {"Definitions": 60, "Sets": 12, "Errors": errors}
