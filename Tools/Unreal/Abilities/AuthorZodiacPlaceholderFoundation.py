# -*- coding: utf-8 -*-
"""SSLM占位资产阶段一：先生成数值表、60个Definition、72个GAS效果及12个UIProfile，不改英雄默认技能。"""
import json
from pathlib import Path
import unreal

import sys
sys.path.insert(0, str(Path(__file__).parent))
import AuthorZodiacPlaceholderAbilities as a

def author_foundation():
    a.check("-SSLMPlaceholderFoundationAuthoring" in unreal.SystemLibrary.get_command_line(),
            "必须显式启用占位基础资源制作")
    a.check(Path(unreal.Paths.get_project_file_path()).resolve() ==
            (a.GAME / "DivineBeastsArena.uproject").resolve(), "不是SSLM正式工程")
    draft = json.loads(a.DRAFT.read_text(encoding="utf-8-sig"))["Entries"]
    icon_map = {(row["HeroDefinitionId"].split(".")[-1], row["Slot"]): row["IconAssetPath"]
                for row in draft if row["Slot"] in ("BasicAttack", "Passive")}
    a.check(len(icon_map) == 24, "缺少12英雄普攻/被动真实图标")
    for hero in a.HEROES:
        asset = unreal.load_asset(a.HERO_ROOT + "/DA_Hero_Zodiac_" + hero)
        wanted = "dba.abilityset." + hero.lower() + "_placeholder@1"
        a.check(isinstance(asset, unreal.DivineBeastsHeroDefinition) and
                str(asset.get_editor_property("DefaultAbilitySetId")) in ("None", wanted),
                hero + "正式英雄已配置其他来源")
        for slot in ("BasicAttack", "Passive"):
            a.check(isinstance(unreal.load_asset(icon_map[(hero, slot)]), unreal.Texture2D),
                    hero + "普攻/被动纹理缺失")
    for mode, path in a.MAGIC_ICONS.items():
        a.check(isinstance(unreal.load_asset(path), unreal.Texture2D),
                mode + "真实魔法图标缺失")
        a.tag("DivineBeasts.Placeholder.Cooldown." + mode)
    for name in ("Primary", "Slot1", "Slot2", "Slot4"):
        a.tag("Platform.Ability.Input.DivineBeasts." + name)
    a.tag("Ability.Active")
    a.tag("Ability.Spell")

    factory = unreal.DataTableFactory()
    factory.set_editor_property("Struct", unreal.load_object(
        None, "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow"))
    table = a.make_asset(a.ROOT + "/DT_DBA_PlaceholderBalance", unreal.DataTable, factory)
    rows = []
    for hero in a.HEROES:
        for mode, label, kind, input_slot, damage, cost, cd, distance, speed, helix, count, interval in a.SLOTS:
            aid = "dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
            rows.append({"Name": hero + "_Placeholder_" + mode + "_L1", "AbilityId": aid,
                         "Level": 1, "BaseDamage": damage,
                         "DamageType": "Physical" if mode == "Primary" else "Untyped" if mode == "Passive" else "Magic",
                         "CooldownSeconds": cd, "MomentumCost": cost,
                         "CastRangeCm": distance, "AreaRadiusCm": 0})
    a.check(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(rows, ensure_ascii=False)), "60行真实数值填充失败")
    a.save(table)

    totals = {"Definitions": 0, "CostGE": 0, "CooldownGE": 0, "UIProfile": 0}
    for hero in a.HEROES:
        profiles = []
        for mode, label, kind, input_slot, damage, cost, cd, distance, speed, helix, count, interval in a.SLOTS:
            aid = "dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
            name = "_DBA_" + hero + "_Placeholder" + mode
            definition = a.make_asset(a.ROOT + "/DA" + name, unreal.DivineBeastsAbilityDefinition,
                                      a.factory_data(unreal.DivineBeastsAbilityDefinition))
            definition.set_editor_property("LogicalId", a.logical(aid))
            definition.set_editor_property("HeroDefinitionId", unreal.Name("Hero.Zodiac." + hero))
            definition.set_editor_property("Kind", getattr(unreal.DivineBeastsAbilityKind, kind))
            definition.set_editor_property("BalanceTable", table)
            definition.set_editor_property("LevelRowNames", [hero + "_Placeholder_" + mode + "_L1"])
            a.save(definition)
            totals["Definitions"] += 1
            if mode in a.COOLDOWNS:
                # 已发布成本GE继续保留为零修改兼容身份，不能因阶段一制作恢复法术扣费。
                bp, cls, cdo = a.blueprint(a.ROOT + "/GE" + name + "Cost", unreal.GameplayEffect)
                cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.INSTANT)
                modifier = unreal.GameplayModifierInfo()
                attr = ('(Attribute="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet:Momentum",'
                        'AttributeOwner="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet",'
                        'AttributeName="Momentum")')
                a.check(modifier.import_text("(Attribute=" + attr + ",ModifierOp=AddBase,ModifierMagnitude=" +
                        a.magnitude(-cost).export_text() + ")"), aid + ":GE气势幅值写入失败")
                cdo.set_editor_property("Modifiers", [modifier] if cost > 0 else [])
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                a.save(bp)
                totals["CostGE"] += 1
            if cd > 0:
                bp, cls, cdo = a.blueprint(a.ROOT + "/GE" + name + "Cooldown", unreal.GameplayEffect)
                cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.HAS_DURATION)
                cdo.set_editor_property("DurationMagnitude", a.magnitude(cd))
                tag_class = unreal.load_class(None, "/Script/GameplayAbilities.TargetTagsGameplayEffectComponent")
                a.check(tag_class is not None, "缺少GAS标签GEComponent")
                components = list(cdo.get_editor_property("GEComponents"))
                component = next((obj for obj in components if obj.get_class() == tag_class), None)
                if component is None:
                    component = unreal.new_object(tag_class, outer=cdo)
                    components.append(component)
                inherited = component.get_editor_property("InheritableGrantedTagsContainer")
                a.check(inherited.import_text("(Added=" +
                        a.tags(["DivineBeasts.Placeholder.Cooldown." + mode]).export_text() + ")"),
                        aid + ":GE冷却标签写入失败")
                component.set_editor_property("InheritableGrantedTagsContainer", inherited)
                cdo.set_editor_property("GEComponents", components)
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                a.save(bp)
                totals["CooldownGE"] += 1

            ui = unreal.DivineBeastsAbilityUIEntry()
            ui.set_editor_property("AbilityId", unreal.Name(aid))
            ui.set_editor_property("DisplayName", unreal.Text(label))
            desc = ("临时未实现被动，不参与自动战斗。" if mode == "Passive" else
                    "SSLM占位技能，服务器校验资格和冷却，法术不消耗气势；后续由正式生肖技能替换。")
            ui.set_editor_property("Description", unreal.Text(desc))
            icon = (icon_map[(hero, "BasicAttack" if mode == "Primary" else "Passive")]
                    if mode in ("Primary", "Passive") else a.MAGIC_ICONS[mode])
            ui.set_editor_property("Icon", unreal.load_asset(icon))
            profiles.append(ui)
        path = "/DBAHeroPack_" + hero + "/UI/Abilities/DA_DBA_" + hero + "_PlaceholderUIProfile"
        profile = a.make_asset(path, unreal.DivineBeastsAbilityUIProfile,
                               a.factory_data(unreal.DivineBeastsAbilityUIProfile))
        profile.set_editor_property("HeroDefinitionId", unreal.Name("Hero.Zodiac." + hero))
        profile.set_editor_property("Entries", profiles)
        a.save(profile)
        totals["UIProfile"] += 1

    # 此阶段有意不修改Hero.DefaultAbilitySetId，也不虚构技能授权已就绪。
    output = a.GAME / "Saved/Validation/VillageSkillPlaceholder20261013/FoundationReport.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"UEAssetsCreated": True, "Totals": totals,
                                  "HeroDefaultsChanged": False,
                                  "GameplayClassesCreated": False,
                                  "NetworkVerified": False,
                                  "Saved": a.SAVED}, ensure_ascii=False, indent=2),
                      encoding="utf-8")
    unreal.log("SSLM_PLACEHOLDER_FOUNDATION=" +
               json.dumps({"Totals": totals, "Report": str(output)}, ensure_ascii=False))

if __name__ == "__main__":
    author_foundation()
