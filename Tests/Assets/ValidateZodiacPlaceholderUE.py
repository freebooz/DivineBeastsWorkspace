# -*- coding: utf-8 -*-
"""SSLM十二生肖占位技能资产UE5.8独立只读冷启动检查；不代表联机、Cook或人工视觉通过。"""
import json
from datetime import datetime, timezone
from pathlib import Path
import unreal

HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
          "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")
MODES = ("Primary", "Passive", "Fireball", "Frostbolt", "ArcaneMissiles", "DirectedMissile")
ROOT = "/DBAGameplay/Abilities/Placeholders"


def inspect():
    # 只读验证正式工程和锁定引擎；禁止把其他项目的同名资产或旧费用合同记为成功。
    assert unreal.Paths.get_project_file_path().replace('\\', '/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
    errors = []
    def check(ok, message):
        if not ok:
            errors.append(message)
    totals = {"heroes": 0, "definitions": 0, "sets": 0, "blueprints": 0,
              "ui_profiles": 0, "grant_ids_verified": 0, "ui_entries_verified": 0,
              "ga_definition_ids_verified": 0, "effects_verified": 0, "zero_cost_magic_rows": 0, "neutral_legacy_cost_effects": 0}
    table = unreal.load_asset(ROOT + "/DT_DBA_PlaceholderBalance")
    check(isinstance(table, unreal.DataTable), "占位数值表不存在")
    if isinstance(table, unreal.DataTable):
        rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
        check(len(rows) == 72, "占位数值表不含实际72行")
        # 真正导出每行数据；只检查行数会漏掉导致TryGetLoadedBalance拒绝施法的旧气势费用。
        balance_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        magic_rows = [row for row in balance_rows if row['DamageType'] == 'Magic']
        check(len(magic_rows) == 48, '占位数值表必须包含48条法术行')
        for row in magic_rows:
            check(row['MomentumCost'] == 0, row['Name'] + ':法术不能消耗气势')
        totals['zero_cost_magic_rows'] = sum(row['MomentumCost'] == 0 for row in magic_rows)
        # Primary保持稳定逻辑ID，但其用户批准行为已替换为20米内20%最大生命治疗；不把旧普攻记为通过。
        primary_rows = [row for row in balance_rows if '_Placeholder_Primary_' in row['Name']]
        check(len(primary_rows) == 12, '治疗必须覆盖十二生肖Primary')
        for row in primary_rows:
            check((row['BaseDamage'], row['DamageType'], row['MomentumCost'], row['CooldownSeconds'], row['CastRangeCm']) ==
                  (0, 'Untyped', 0, 10, 2000), row['Name'] + ':治疗数值合同不符')

    def logical(obj):
        return (str(obj.get_editor_property("Namespace")) + "." +
                str(obj.get_editor_property("Name")) + "@" +
                str(obj.get_editor_property("LogicalVersion")))
    for hero in HEROES:
        hero_id = "Hero.Zodiac." + hero
        set_id = "dba.abilityset." + hero.lower() + "_placeholder@1"
        identity = unreal.load_asset("/DBAGameplay/Definitions/DA_Hero_Zodiac_" + hero)
        ability_set = unreal.load_asset(ROOT + "/DA_DBA_" + hero + "_PlaceholderAbilitySet")
        ui = unreal.load_asset("/DBAHeroPack_" + hero + "/UI/Abilities/DA_DBA_" +
                               hero + "_PlaceholderUIProfile")
        check(isinstance(identity, unreal.DivineBeastsHeroDefinition), hero + ":英雄资产不存在")
        if identity:
            check(str(identity.get_editor_property("DefaultAbilitySetId")) == set_id,
                  hero + ":默认技能未接入")
            totals["heroes"] += 1
        check(isinstance(ability_set, unreal.GamePlatformAbilitySetDefinition), hero + ":技能集合不存在")
        if ability_set:
            totals["sets"] += 1
            check(not ability_set.get_editor_property("bDevelopmentOnly"),
                  hero + ":仍被开发门禁限制")
            grants = list(ability_set.get_editor_property("Abilities"))
            required = list(ability_set.get_editor_property("RequiredDefinitions"))
            check(len(grants) == 6 and len(required) == 6, hero + ":五个主动技能加一个被动的授权不完整")
            expected_ids = {"dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
                            for mode in MODES}
            grant_ids = {logical(g.get_editor_property("AbilityId")) for g in grants}
            depend_ids = {str(r.get_editor_property("PrimaryAssetName")) for r in required}
            check(grant_ids == expected_ids, hero + ":授予技能身份不完整或出现重复")
            check(depend_ids == expected_ids, hero + ":技能定义预热依赖缺失")
            totals["grant_ids_verified"] += len(grant_ids & expected_ids)
            for mode, grant in zip(MODES, grants):
                class_ref = str(grant.get_editor_property("AbilityClass"))
                check("GA_DBA_" + hero + "_Placeholder" + mode in class_ref,
                      hero + "/" + mode + ":授权的真实GA类与技能身份不匹配")
                expected_input = {'Primary':'Slot3','Passive':'None','Fireball':'Slot1','Frostbolt':'Slot2','ArcaneMissiles':'Slot4','DirectedMissile':'Primary'}[mode]
                check(str(grant.get_editor_property('InputTag').get_editor_property('TagName')) ==
                      ('None' if expected_input=='None' else 'Platform.Ability.Input.DivineBeasts.'+expected_input),hero+'/'+mode+':真实按钮输入授权错误')
        check(isinstance(ui, unreal.DivineBeastsAbilityUIProfile), hero + ":UIProfile不存在")
        if ui:
            totals["ui_profiles"] += 1
            entries = list(ui.get_editor_property("Entries"))
            check(len(entries) == 6, hero + ":客户端图标配置不完整")
            expected_ui_ids = {"dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
                               for mode in MODES}
            entry_ids = {str(entry.get_editor_property("AbilityId")) for entry in entries}
            check(entry_ids == expected_ui_ids, hero + ":UI技能身份无法与GAS授权匹配")
            for entry in entries:
                check(bool(str(entry.get_editor_property("DisplayName")).strip()) and
                      bool(str(entry.get_editor_property("Description")).strip()),
                      hero + ":占位技能缺少真实中文名称和说明")
                check("None" not in str(entry.get_editor_property("Icon")),
                      hero + ":软引用图标尚未指向真实素材")
            totals["ui_entries_verified"] += len(entry_ids & expected_ui_ids)
        for mode in MODES:
            path = ROOT + "/_DBA_" + hero + "_Placeholder" + mode
            definition = unreal.load_asset(ROOT + "/DA" + path.rsplit("/", 1)[-1])
            check(isinstance(definition, unreal.DivineBeastsAbilityDefinition),
                  hero + "/" + mode + ":定义不存在")
            totals["definitions"] += int(isinstance(definition, unreal.DivineBeastsAbilityDefinition))
            ability_id = "dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
            if isinstance(definition, unreal.DivineBeastsAbilityDefinition):
                check(logical(definition.get_editor_property("LogicalId")) == ability_id,
                      ability_id + ":真实技能逻辑ID不匹配")
                check(str(definition.get_editor_property("HeroDefinitionId")) == hero_id,
                      ability_id + ":实际技能归属生肖不一致")
                check(len(definition.get_editor_property("LevelRowNames")) == 1,
                      ability_id + ":缺少一级占位数值")
            cls = unreal.EditorAssetLibrary.load_blueprint_class(
                ROOT + "/GA" + path.rsplit("/", 1)[-1])
            ok = cls is not None and isinstance(unreal.get_default_object(cls),
                                                   unreal.DivineBeastsPlaceholderGameplayAbility)
            check(ok, hero + "/" + mode + ":技能执行蓝图父类非法")
            totals["blueprints"] += int(ok)
            if ok:
                cdo = unreal.get_default_object(cls)
                pid = cdo.get_editor_property("AbilityDefinitionId")
                check(str(pid.get_editor_property("PrimaryAssetName")) == ability_id,
                      ability_id + ":真实GAS执行器主资产引用不匹配")
                totals["ga_definition_ids_verified"] += 1
                cost = cdo.get_editor_property("CostGameplayEffectClass")
                cooldown = cdo.get_editor_property("CooldownGameplayEffectClass")
                requires_effects = mode != "Passive"
                check(not cost and bool(cooldown) == requires_effects,
                      ability_id + ":法术成本必须为空，原有冷却GE必须保留")
                totals["effects_verified"] += int(requires_effects and not cost and bool(cooldown))
                if mode == "Primary":
                    check('Ability.Spell' in cdo.get_editor_property('AbilityTags').export_text(),ability_id+':治疗缺少法术语义，可能绕过沉默')
                    check(cdo.get_editor_property('PlaceholderMode') == unreal.DivineBeastsPlaceholderAbilityMode.HEALING,
                          ability_id + ':Primary仍未迁移治疗执行模式')
                if mode == 'DirectedMissile':
                    check(str(cdo.get_editor_property('PresentationStyleOverride'))=='DBA.Development.DirectedMissile',ability_id+':独立定向飞弹表现未接通')
                if mode not in ("Primary", "Passive"):
                    legacy_path = ROOT + "/GE" + path.rsplit("/", 1)[-1] + "Cost"
                    legacy_cls = unreal.EditorAssetLibrary.load_blueprint_class(legacy_path)
                    neutral = legacy_cls is not None and not unreal.get_default_object(legacy_cls).get_editor_property('Modifiers')
                    check(neutral, ability_id + ':旧成本GE稳定身份必须保留为无修改兼容资产')
                    totals['neutral_legacy_cost_effects'] += int(neutral)
    result = {"Status": "PASS" if not errors else "FAIL", "Counts": totals,
              "Errors": errors, "NetworkVerified": False, "CookVerified": False}
    report = Path(unreal.Paths.project_saved_dir()) / "Validation" / "VillageSkillPlaceholder20261013" / ("ColdReadback_" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".json")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SSLM_PLACEHOLDER_READBACK=" +
               json.dumps({"Status": result["Status"], "Counts": totals,
                           "Errors": errors[:12]}, ensure_ascii=False))
    if errors:
        raise RuntimeError("占位技能UE资产冷读失败: " + str(len(errors)))


if __name__ == "__main__":
    inspect()
