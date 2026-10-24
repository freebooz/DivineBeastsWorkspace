# -*- coding: utf-8 -*-
"""
SSLM十二生肖占位技能原生资产作者：通过UE5.8 Editor Python生成真实资产，不制作文本伪uasset。
产出：12英雄 × (6份技能定义 + 6份独立GAS蓝图 + 1份授权集 + 1份客户端UIProfile)
     +48兼容成本GE+60冷却GE + 1份72行数值表。
默认数字1定向飞弹、2火焰弹、3寒冰箭、4奥术飞弹、5治疗，另保留被动；治疗Primary身份不改。
资产先完整生成/校验，最后才配置Hero.DefaultAbilitySetId；不存在开发技能后门。
仅在 -SSLMPlaceholderAuthoring 显式许可时执行。不得由普通游戏启动自动执行。
所有代码、引擎类和资源只属于SSLM项目层；平台机制保持原状。
"""
from __future__ import annotations

import json
import sys
from pathlib import Path
from datetime import datetime, timezone

import unreal

WORKSPACE = Path(__file__).resolve().parents[3]
GAME = WORKSPACE / "Game"
ROOT = "/DBAGameplay/Abilities/Placeholders"
HERO_ROOT = "/DBAGameplay/Definitions"
HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
          "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")
SLOTS = (
    ("Primary", "花瓣绽放", "PRIMARY", "SLOT3", 0, 0, 10, 2000, 0, 0, 1, 0),
    ("Passive", "被动技能（占位）", "PASSIVE", "", 0, 0, 0, 0, 0, 0, 1, 0),
    # 2026-10-24用户要求快速飞行：厘米/秒恒速，不增加起飞加速阶段；其它平衡参数沿用原值。
    ("Fireball", "火焰弹（占位）", "ACTIVE", "SLOT1", 40, 0, 2, 5000, 6000, 0, 1, 0),
    ("Frostbolt", "寒冰箭（占位）", "ACTIVE", "SLOT2", 30, 0, 1.5, 5000, 8000, 0, 1, 0),
    ("ArcaneMissiles", "奥术飞弹（占位）", "ULTIMATE", "SLOT4", 12, 0, 3, 5000, 7000, 18, 3, .2),
    ("DirectedMissile", "定向飞弹", "ACTIVE", "PRIMARY", 40, 0, 2, 5000, 9000, 0, 1, 0),
)
MAGIC_SLOTS = ("Fireball", "Frostbolt", "ArcaneMissiles", "DirectedMissile")
COOLDOWNS = ("Primary",) + MAGIC_SLOTS
MAGIC_ICONS = {
    name: "/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev" + name
    for name in MAGIC_SLOTS
}
MAGIC_ICONS["DirectedMissile"] = MAGIC_ICONS["Fireball"]
DRAFT = WORKSPACE / "Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json"
SAVED = []
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def check(condition, label):
    if not condition:
        raise RuntimeError(label)


def make_asset(path, cls, factory):
    """只允许新建或幂等复用同种资产，禁止覆盖任何用户改名/改父类的资源。"""
    # 资产首次制作时先查询存在性；直接LoadAsset会把正常的新建路径误记为引擎Error，
    # 让PythonCommandlet最终返回非零退出码，即使真实保存全部成功。
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
    else:
        name, folder = path.rsplit("/", 1)[1], path.rsplit("/", 1)[0]
        asset = ASSET_TOOLS.create_asset(name, folder, cls, factory)
    check(isinstance(asset, cls), "类型或创建错误：" + path)
    return asset


def save(asset):
    check(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False),
          "UE资产未真正保存：" + asset.get_path_name())
    SAVED.append(asset.get_path_name())


def logical(identity):
    namespace, rhs = identity.rsplit(".", 1)
    name, version = rhs.rsplit("@", 1)
    item = unreal.GamePlatformId()
    item.set_editor_property("Namespace", namespace)
    item.set_editor_property("Name", name)
    item.set_editor_property("LogicalVersion", int(version))
    return item


def primary(identity):
    asset_type = unreal.PrimaryAssetType()
    asset_type.set_editor_property("Name", "GamePlatformDefinition")
    item = unreal.PrimaryAssetId()
    item.set_editor_property("PrimaryAssetType", asset_type)
    item.set_editor_property("PrimaryAssetName", identity)
    return item


def tag(name):
    item = unreal.GameplayTag()
    if name:
        check(item.import_text(name) and unreal.GameplayTagLibrary.is_gameplay_tag_valid(item),
              "未注册双端GameplayTag：" + name)
    return item


def tags(names):
    value = unreal.GameplayTagContainer()
    for name in names:
        tag(name)
    if names:
        check(value.import_text('(GameplayTags=(' +
              ','.join('(TagName="' + name + '")' for name in names) + '))'),
              "标签结构创建失败")
    return value


def factory_data(cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("DataAssetClass", cls)
    return factory


def blueprint(path, parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("ParentClass", parent)
    bp = make_asset(path, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    check(cls is not None and isinstance(unreal.get_default_object(cls), parent),
          "蓝图父类不是本轮受控类：" + path)
    return bp, cls, unreal.get_default_object(cls)


def magnitude(number):
    m = unreal.GameplayEffectModifierMagnitude()
    check(m.import_text("(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=" +
                        str(float(number)) + "))"), "GAS静态幅值创建失败")
    return m


def preflight():
    cmd = unreal.SystemLibrary.get_command_line()
    check("-SSLMPlaceholderAuthoring" in cmd, "缺少-SSLMPlaceholderAuthoring显式许可")
    check(Path(unreal.Paths.get_project_file_path()).resolve() == (GAME / "DivineBeastsArena.uproject").resolve(),
          "非正式SSLM工程，禁止写入")
    check(hasattr(unreal, "DivineBeastsPlaceholderGameplayAbility"),
          "新占位原生类尚未构建/加载；需先UE5.8真实编译")
    check(unreal.load_object(None, "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow") is not None,
          "SSLM数值行结构不存在")
    candidate = json.loads(DRAFT.read_text(encoding="utf-8-sig"))["Entries"]
    check(len(candidate) == 60, "历史候选必须完整，不能仅凭槽位假定图标")
    icons = {}
    for row in candidate:
        hero = row["HeroDefinitionId"].split(".")[-1]
        if row["Slot"] in ("BasicAttack", "Passive"):
            icons[(hero, row["Slot"])] = row["IconAssetPath"]
    check(len(icons) == 24, "普通攻击/被动图标不完整")
    for (hero, slot), path in icons.items():
        check(isinstance(unreal.load_asset(path), unreal.Texture2D),
              f"{hero} {slot} 真实Texture2D缺失: {path}")
    for name, path in MAGIC_ICONS.items():
        check(isinstance(unreal.load_asset(path), unreal.Texture2D),
              "临时魔法图标缺失：" + name)
        tag("DivineBeasts.Placeholder.Cooldown." + name)
    for input_name in ("Primary", "Slot1", "Slot2", "Slot3", "Slot4"):
        tag("Platform.Ability.Input.DivineBeasts." + input_name)
    tag("Ability.Active")
    tag("Ability.Spell")
    heroes = {}
    for hero in HEROES:
        asset = unreal.load_asset(HERO_ROOT + "/DA_Hero_Zodiac_" + hero)
        check(isinstance(asset, unreal.DivineBeastsHeroDefinition), "正式英雄定义缺失：" + hero)
        proposed = "dba.abilityset." + hero.lower() + "_placeholder@1"
        old = str(asset.get_editor_property("DefaultAbilitySetId"))
        check(old in ("None", proposed),
              f"{hero}已经配置其他技能集合，不能覆盖用户的新设计：" + old)
        heroes[hero] = asset
    return heroes, icons


def author():
    heroes, icons = preflight()
    # 先创建正式技能数据区，避免依赖开发目录在Shipping/Test被NeverCook截断。
    table_path = ROOT + "/DT_DBA_PlaceholderBalance"
    factory = unreal.DataTableFactory()
    factory.set_editor_property(
        "Struct", unreal.load_object(None, "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow"))
    table = make_asset(table_path, unreal.DataTable, factory)
    rows = []
    for hero in HEROES:
        for mode, _label, _kind, _input, damage, cost, cooldown, distance, _speed, _helix, _count, _interval in SLOTS:
            identity = "dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
            rows.append({
                "Name": hero + "_Placeholder_" + mode + "_L1",
                "AbilityId": identity, "Level": 1,
                "BaseDamage": damage, "DamageType": "Untyped" if mode == "Primary" else
                    ("Untyped" if mode == "Passive" else "Magic"),
                "CooldownSeconds": cooldown, "MomentumCost": cost,
                "CastRangeCm": distance, "AreaRadiusCm": 0
            })
    check(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(rows, ensure_ascii=False)), "一级数值表真实填充失败")
    save(table)

    summary = []
    for hero in HEROES:
        hid = "Hero.Zodiac." + hero
        grants, deps, ui_entries = [], [], []
        for mode, label, kind, input_slot, damage, cost, cooldown, distance, speed, helix, count, interval in SLOTS:
            identity = "dba.ability." + hero.lower() + "_placeholder_" + mode.lower() + "@1"
            base_name = "_DBA_" + hero + "_Placeholder" + mode
            definition = make_asset(
                ROOT + "/DA" + base_name, unreal.DivineBeastsAbilityDefinition,
                factory_data(unreal.DivineBeastsAbilityDefinition))
            definition.set_editor_property("LogicalId", logical(identity))
            definition.set_editor_property("HeroDefinitionId", unreal.Name(hid))
            definition.set_editor_property("Kind", getattr(unreal.DivineBeastsAbilityKind, kind))
            definition.set_editor_property("BalanceTable", table)
            definition.set_editor_property("LevelRowNames", [hero + "_Placeholder_" + mode + "_L1"])
            save(definition)
            cost_cls = None
            cooldown_cls = None
            cooldown_tags = []
            if mode in MAGIC_SLOTS:
                # 保留已发布成本GE身份为无修改兼容资产；法术GA不再引用它，冷却仍独立制作。
                cost_bp, _temp, cost_cdo = blueprint(ROOT + "/GE" + base_name + "Cost", unreal.GameplayEffect)
                cost_cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.INSTANT)
                attr = ('(Attribute="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet:Momentum",'
                        'AttributeOwner="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet",'
                        'AttributeName="Momentum")')
                mod = unreal.GameplayModifierInfo()
                check(mod.import_text("(Attribute=" + attr +
                    ",ModifierOp=AddBase,ModifierMagnitude=" + magnitude(-cost).export_text() + ")"),
                    "真实气势GAS成本设置失败：" + identity)
                cost_cdo.set_editor_property("Modifiers", [mod] if cost > 0 else [])
                unreal.BlueprintEditorLibrary.compile_blueprint(cost_bp)
                save(cost_bp)
                compatibility_cost_cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + base_name + "Cost")
                check(compatibility_cost_cls is not None, "成本效果无真实生成类：" + identity)
                cost_cls = compatibility_cost_cls if cost > 0 else None
            if cooldown > 0:
                cool_bp, _temp, cool_cdo = blueprint(ROOT + "/GE" + base_name + "Cooldown", unreal.GameplayEffect)
                cool_cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.HAS_DURATION)
                cool_cdo.set_editor_property("DurationMagnitude", magnitude(cooldown))
                cdtag = "DivineBeasts.Placeholder.Cooldown." + mode
                cooldown_tags = [cdtag]
                # UE5.8 GameplayEffect通过真实GEComponent声明标签，不使用失效的GrantedTags属性。
                tag_class = unreal.load_class(None, "/Script/GameplayAbilities.TargetTagsGameplayEffectComponent")
                check(tag_class is not None, "UE5.8冷却标签组件类不可用")
                components = list(cool_cdo.get_editor_property("GEComponents"))
                component = next((obj for obj in components if obj.get_class() == tag_class), None)
                if component is None:
                    component = unreal.new_object(tag_class, outer=cool_cdo)
                    components.append(component)
                inherited = component.get_editor_property("InheritableGrantedTagsContainer")
                check(inherited.import_text("(Added=" + tags(cooldown_tags).export_text() + ")"),
                      "GE冷却标签导入失败：" + identity)
                component.set_editor_property("InheritableGrantedTagsContainer", inherited)
                cool_cdo.set_editor_property("GEComponents", components)
                unreal.BlueprintEditorLibrary.compile_blueprint(cool_bp)
                save(cool_bp)
                cooldown_cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + base_name + "Cooldown")
                check(cooldown_cls is not None, "冷却效果无真实生成类：" + identity)

            ga_path = ROOT + "/GA" + base_name
            ga_bp, _temp, cdo = blueprint(ga_path, unreal.DivineBeastsPlaceholderGameplayAbility)
            cdo.set_editor_property("AbilityDefinitionId", primary(identity))
            cdo.set_editor_property("PlaceholderMode",
                getattr(unreal.DivineBeastsPlaceholderAbilityMode, "FIREBALL" if mode == "DirectedMissile" else "HEALING" if mode == "Primary" else mode.upper() if mode != "ArcaneMissiles"
                    else "ARCANE_MISSILES"))
            cdo.set_editor_property("PresentationStyleOverride", "DBA.Development.DirectedMissile" if mode == "DirectedMissile" else "None")
            motion = unreal.GamePlatformCombatProjectileMotion()
            if speed > 0:
                for key, val in {
                    "SpeedCmPerSecond": speed, "RangeCm": distance,
                    "CollisionRadiusCm": 8, "HelixRadiusCm": helix,
                    "HelixTurnsPerSecond": 3 if helix else 0,
                    "InitialPhaseTurns": 0
                }.items():
                    motion.set_editor_property(key, val)
            cdo.set_editor_property("ProjectileMotion", motion)
            cdo.set_editor_property("ProjectileCount", count)
            cdo.set_editor_property("ProjectileIntervalSeconds", interval)
            cdo.set_editor_property("CostGameplayEffectClass", cost_cls)
            cdo.set_editor_property("CooldownGameplayEffectClass", cooldown_cls)
            cdo.set_editor_property("PlaceholderCooldownTags", tags(cooldown_tags))
            cdo.set_editor_property("AbilityTags",
                                    tags([] if mode == "Passive" else
                                    ["Ability.Active", "Ability.Spell"]))
            unreal.BlueprintEditorLibrary.compile_blueprint(ga_bp)
            save(ga_bp)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(ga_path)
            check(cls is not None and isinstance(unreal.get_default_object(cls),
                        unreal.DivineBeastsPlaceholderGameplayAbility),
                  "能力授权类失效：" + identity)
            input_tag = "" if not input_slot else "Platform.Ability.Input.DivineBeasts." + (
                input_slot[0] + input_slot[1:].lower() if input_slot == "PRIMARY"
                else "Slot" + input_slot[-1])
            grant = unreal.GamePlatformAbilityGrant()
            check(grant.import_text(
                '(AbilityId=' + logical(identity).export_text() +
                ',AbilityClass="' + cls.get_path_name() +
                '",AbilityLevel=1,InputTag=' + tag(input_tag).export_text() + ')'),
                "AbilitySet授权结构不可解析：" + identity)
            grants.append(grant)
            deps.append(primary(identity))
            item = unreal.DivineBeastsAbilityUIEntry()
            item.set_editor_property("AbilityId", unreal.Name(identity))
            item.set_editor_property("DisplayName", unreal.Text(label))
            if mode == "Passive":
                description = "此为被动技能占位，暂不生效；后续由独立生肖被动机制替换。"
            elif mode == "Primary":
                description = "恢复自己或当前选中有效友方20%最大生命，射程20米，冷却10秒，不消耗气势。友方死亡、超距或化身失效时拒绝施法，不复活。"
            else:
                description = (
                    "临时" + label + "：每枚基础伤害" + str(damage) +
                    "，法术不消耗气势，冷却" + str(cooldown) +
                    "秒。服务器权威弹体" + ("三连发。" if count == 3 else "单发。") +
                    "未实现减速/燃烧等最终生肖效果，后续直接替换对应技能定义及执行类。")
            item.set_editor_property("Description", unreal.Text(description))
            icon_path = (icons[(hero, "BasicAttack" if mode == "Primary" else "Passive")]
                         if mode in ("Primary", "Passive") else MAGIC_ICONS[mode])
            item.set_editor_property("Icon", unreal.load_asset(icon_path))
            ui_entries.append(item)

        set_id = "dba.abilityset." + hero.lower() + "_placeholder@1"
        ability_set = make_asset(
            ROOT + "/DA_DBA_" + hero + "_PlaceholderAbilitySet",
            unreal.GamePlatformAbilitySetDefinition,
            factory_data(unreal.GamePlatformAbilitySetDefinition))
        ability_set.set_editor_property("LogicalId", logical(set_id))
        ability_set.set_editor_property("bDevelopmentOnly", False)
        ability_set.set_editor_property("Abilities", grants)
        ability_set.set_editor_property("RequiredDefinitions", deps)
        ability_set.set_editor_property("Effects", [])
        ability_set.set_editor_property("Attributes", [])
        save(ability_set)

        profile_path = ("/DBAHeroPack_" + hero +
                        "/UI/Abilities/DA_DBA_" + hero + "_PlaceholderUIProfile")
        profile = make_asset(profile_path, unreal.DivineBeastsAbilityUIProfile,
                             factory_data(unreal.DivineBeastsAbilityUIProfile))
        profile.set_editor_property("HeroDefinitionId", unreal.Name(hid))
        profile.set_editor_property("Entries", ui_entries)
        save(profile)
        summary.append({"Hero": hero, "SetId": set_id, "Abilities": len(grants),
                        "RequiredDefinitions": len(deps),
                        "UIProfile": profile.get_path_name()})

    # 临时数据独立于官方设计：12套技能集及UI都保存成功后才写Hero默认集合。
    for hero, asset in heroes.items():
        set_id = "dba.abilityset." + hero.lower() + "_placeholder@1"
        check(str(asset.get_editor_property("DefaultAbilitySetId")) in ("None", set_id),
              hero + "在作者阶段被外部修改，拒绝覆盖")
    for hero, asset in heroes.items():
        asset.set_editor_property("DefaultAbilitySetId",
                                  unreal.Name("dba.abilityset." + hero.lower() + "_placeholder@1"))
        save(asset)

    report = {
        "Status": "UEAssetsWrittenPendingColdReadbackAndRuntimeNetworkVerification",
        "Date": datetime.now(timezone.utc).isoformat(),
        "Project": "SSLM", "HeroCount": len(heroes), "AbilityDefinitionCount": 72,
        "GameplayAbilityBlueprintCount": 72, "PlaceholderAbilitySetCount": 12,
        "ClientUIProfileCount": 12, "CostEffectCount": 48,
        "CooldownEffectCount": 60, "BalanceRowCount": 72,
        "FormalNamesAndMechanicsApproved": False,
        "DedicatedServerGrantVerified": False,
        "ClientCookVerified": False, "ServerCookVerified": False,
        "ClientVFXVerified": False, "SavedPaths": SAVED, "Heroes": summary,
    }
    report_path = GAME / "Saved/Validation/VillageSkillPlaceholder20261013/AuthorReport.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SSLM_PLACEHOLDER_ASSETS_WRITTEN=" +
               json.dumps({"Heroes": len(heroes), "SavedCount": len(SAVED),
                           "Report": str(report_path)}, ensure_ascii=False))
    return report


def repair_spell_momentum_costs():
    """显式Monolith迁移既有36法术的零费合同；不重建英雄、技能、冷却或修改伤害。

    仅用户已授权的正式Editor游戏线程调用。先验证全部对象并记录原表、GE、GA及UI文字，
    然后保存原身份；异常保留已完成证据，不伪造事务回滚。调用前由主流程备份真实uasset。
    """
    check(Path(unreal.Paths.get_project_file_path()).resolve() == (GAME / "DivineBeastsArena.uproject").resolve(),
          "非正式SSLM工程，拒绝迁移")
    table = unreal.load_asset(ROOT + "/DT_DBA_PlaceholderBalance")
    check(isinstance(table, unreal.DataTable), "真实占位数值表缺失")
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    check(len(rows) == 60, "占位表不是已批准的60行，拒绝覆盖")
    expected = {hero + "_Placeholder_" + mode + "_L1" for hero in HEROES for mode in MAGIC_SLOTS}
    magic_rows = [row for row in rows if row["Name"] in expected]
    check(len(magic_rows) == 36 and all(row["DamageType"] == "Magic" for row in magic_rows), "法术行身份异常")
    effects, abilities, profiles = [], [], []
    for hero in HEROES:
        for mode in MAGIC_SLOTS:
            suffix = "_DBA_" + hero + "_Placeholder" + mode
            ge = unreal.load_asset(ROOT + "/GE" + suffix + "Cost")
            ga = unreal.load_asset(ROOT + "/GA" + suffix)
            check(isinstance(ge, unreal.Blueprint) and isinstance(ga, unreal.Blueprint), "旧GA或兼容GE缺失：" + suffix)
            ge_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + suffix + "Cost"))
            mods = list(ge_cdo.get_editor_property("Modifiers"))
            check(len(mods) <= 1 and all('Momentum' in mod.get_editor_property('Attribute').export_text() for mod in mods),
                  "成本GE含非既定修改，拒绝清除：" + suffix)
            effects.append((ge, ge_cdo, [mod.export_text() for mod in mods]))
            abilities.append(ga)
        profile = unreal.load_asset("/DBAHeroPack_" + hero + "/UI/Abilities/DA_DBA_" + hero + "_PlaceholderUIProfile")
        check(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), "真实英雄UI配置缺失：" + hero)
        profiles.append(profile)
    folder = GAME / "Saved/Validation/SpellMomentumMigration"
    folder.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    before = {"Rows": rows, "Effects": [{"Path": ge.get_path_name(), "Modifiers": old} for ge, _cdo, old in effects],
              "Descriptions": {profile.get_path_name(): [str(entry.get_editor_property('Description'))
                              for entry in profile.get_editor_property('Entries')] for profile in profiles}}
    (folder / (stamp + "_Before.json")).write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding="utf-8")
    for row in magic_rows:
        row["MomentumCost"] = 0
    check(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)), "原表零费填充失败")
    save(table)
    for ge, cdo, _old in effects:
        cdo.set_editor_property("Modifiers", [])
        unreal.BlueprintEditorLibrary.compile_blueprint(ge)
        save(ge)
    for ga in abilities:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(ga.get_path_name().split('.')[0])
        unreal.get_default_object(cls).set_editor_property("CostGameplayEffectClass", None)
        unreal.BlueprintEditorLibrary.compile_blueprint(ga)
        save(ga)
    for profile in profiles:
        entries = list(profile.get_editor_property("Entries"))
        for entry in entries:
            text = str(entry.get_editor_property("Description"))
            import re
            text = re.sub(r"气势消耗(?:10|8|12)(?:\.0)?", "法术不消耗气势", text)
            entry.set_editor_property("Description", unreal.Text(text))
        profile.set_editor_property("Entries", entries)
        save(profile)
    report = {"Status": "SavedPendingNativeReloadAndGASRegression", "MagicRows":36, "NeutralCostEffects":36,
              "ClearedAbilityCostReferences":36, "UIProfiles":12, "SavedPaths":SAVED, "NativeGASVerified":False}
    path = folder / (stamp + "_Saved.json")
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("SSLM_SPELL_ZERO_COST_SAVED", str(path))
    return report


def migrate_primary_to_healing():
    """由已授权Monolith游戏线程显式调用的幂等迁移；普通模块导入绝不执行。

    先读取全部真实对象并验证稳定身份，保留12个Primary AbilityId、输入授权及UI条目身份。
    仅修改12行数值/GA/UI文字并创建原生GAS冷却；花瓣VFX由项目表现目录单独接入，保留图标和其它槽位。
    调用前主任务须备份真实资产；保存失败保留错误和审计，不声称多资产事务已回滚。
    """
    check(Path(unreal.Paths.get_project_file_path()).resolve() == (GAME / "DivineBeastsArena.uproject").resolve(),
          "非正式SSLM工程，拒绝迁移")
    check(hasattr(unreal.DivineBeastsPlaceholderAbilityMode, "HEALING"), "治疗原生枚举尚未构建/加载")
    cooldown_tag = "DivineBeasts.Placeholder.Cooldown.Primary"
    tag(cooldown_tag)
    table = unreal.load_asset(ROOT + "/DT_DBA_PlaceholderBalance")
    check(isinstance(table, unreal.DataTable), "真实占位数值表缺失")
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    check(len(rows) == 60, "占位表不是60行，拒绝覆盖")
    entries = []
    for hero in HEROES:
        suffix = "_DBA_" + hero + "_PlaceholderPrimary"
        identity = "dba.ability." + hero.lower() + "_placeholder_primary@1"
        matching = [row for row in rows if row["Name"] == hero + "_Placeholder_Primary_L1"]
        check(len(matching) == 1 and matching[0]["AbilityId"] == identity, "Primary表行身份异常：" + hero)
        row = matching[0]
        check((row["BaseDamage"], row["MomentumCost"], row["CooldownSeconds"], row["CastRangeCm"]) in
              ((40, 0, 0, 250), (0, 0, 10, 2000)), "Primary已被另行设计，拒绝覆盖：" + hero)
        definition = unreal.load_asset(ROOT + "/DA" + suffix)
        bp = unreal.load_asset(ROOT + "/GA" + suffix)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GA" + suffix)
        cdo = unreal.get_default_object(cls) if cls else None
        check(isinstance(definition, unreal.DivineBeastsAbilityDefinition) and isinstance(bp, unreal.Blueprint) and
              isinstance(cdo, unreal.DivineBeastsPlaceholderGameplayAbility), "Primary真实资产/父类缺失：" + hero)
        # UE结构体的str含对象地址；比较引擎稳定导出内容，避免合法Primary身份被不同临时地址误拒绝。
        check(cdo.get_editor_property("AbilityDefinitionId").export_text() == primary(identity).export_text(),
              "GA主资产身份异常：" + hero)
        check(cdo.get_editor_property("PlaceholderMode") in (unreal.DivineBeastsPlaceholderAbilityMode.PRIMARY,
              unreal.DivineBeastsPlaceholderAbilityMode.HEALING), "GA已改为其他机制，拒绝覆盖：" + hero)
        profile = unreal.load_asset("/DBAHeroPack_" + hero + "/UI/Abilities/DA_DBA_" + hero + "_PlaceholderUIProfile")
        check(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), "真实UIProfile缺失：" + hero)
        ui_entries = list(profile.get_editor_property("Entries"))
        ui = [entry for entry in ui_entries if str(entry.get_editor_property("AbilityId")) == identity]
        check(len(ui) == 1, "UI Primary身份不唯一：" + hero)
        entries.append((hero, suffix, row, bp, cdo, profile, ui_entries, ui[0]))
    folder = GAME / "Saved/Validation/PrimaryHealingMigration"
    folder.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    (folder / (stamp + "_Before.json")).write_text(json.dumps({"Rows": rows, "Abilities": [
        {"Path": bp.get_path_name(), "Mode": str(cdo.get_editor_property("PlaceholderMode")),
         "Name": str(ui.get_editor_property("DisplayName")), "Description": str(ui.get_editor_property("Description"))}
        for _hero, _suffix, _row, bp, cdo, _profile, _ui_entries, ui in entries]}, ensure_ascii=False, indent=2), encoding="utf-8")
    saved_before = len(SAVED)
    for hero, suffix, row, bp, cdo, profile, ui_entries, ui in entries:
        cool_bp, _cls, cool_cdo = blueprint(ROOT + "/GE" + suffix + "Cooldown", unreal.GameplayEffect)
        cool_cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.HAS_DURATION)
        cool_cdo.set_editor_property("DurationMagnitude", magnitude(10))
        tag_class = unreal.load_class(None, "/Script/GameplayAbilities.TargetTagsGameplayEffectComponent")
        check(tag_class is not None, "UE5.8冷却标签组件不可用")
        components = list(cool_cdo.get_editor_property("GEComponents"))
        component = next((obj for obj in components if obj.get_class() == tag_class), None)
        if component is None:
            component = unreal.new_object(tag_class, outer=cool_cdo)
            components.append(component)
        inherited = component.get_editor_property("InheritableGrantedTagsContainer")
        check(inherited.import_text("(Added=" + tags([cooldown_tag]).export_text() + ")"), "治疗冷却标签导入失败")
        component.set_editor_property("InheritableGrantedTagsContainer", inherited)
        cool_cdo.set_editor_property("GEComponents", components)
        unreal.BlueprintEditorLibrary.compile_blueprint(cool_bp)
        save(cool_bp)
        cooldown_class = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + suffix + "Cooldown")
        check(cooldown_class is not None, "治疗冷却生成类缺失：" + hero)
        cdo.set_editor_property("PlaceholderMode", unreal.DivineBeastsPlaceholderAbilityMode.HEALING)
        cdo.set_editor_property("AbilityTags", tags(["Ability.Active", "Ability.Spell"]))
        cdo.set_editor_property("CostGameplayEffectClass", None)
        cdo.set_editor_property("CooldownGameplayEffectClass", cooldown_class)
        cdo.set_editor_property("PlaceholderCooldownTags", tags([cooldown_tag]))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        row.update(BaseDamage=0, DamageType="Untyped", MomentumCost=0, CooldownSeconds=10, CastRangeCm=2000)
        ui.set_editor_property("DisplayName", unreal.Text("花瓣绽放"))
        ui.set_editor_property("Description", unreal.Text("恢复自己或当前选中有效友方20%最大生命，射程20米，冷却10秒，不消耗气势。友方死亡、超距或化身失效时拒绝施法，不复活。"))
        profile.set_editor_property("Entries", ui_entries)
        save(profile)
    check(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)), "治疗表行写入失败")
    save(table)
    report = {"Status": "SavedPendingColdReloadServerGASAndClientVisualVerification", "PrimaryCount": 12,
              "MaxHealthFraction": .2, "CooldownSeconds": 10, "MomentumCost": 0, "CastRangeCm": 2000,
              "SavedPaths": SAVED[saved_before:], "LeafVFXChanged": False, "RuntimeVerified": False}
    (folder / (stamp + "_Saved.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("SSLM_PRIMARY_HEALING_SAVED", json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    try:
        author()
    except Exception as error:
        unreal.log_error("SSLM_PLACEHOLDER_AUTHORING_FAILED=" + str(error))
        raise


def migrate_attack_range_50m():
    """Monolith窄迁移：108条攻击数值和对应运动参数改为5000厘米，治疗/被动及其它字段不变。

    只处理三张既有占位/开发表；先检查全部身份并备份原始包，遇到人工不同参数即停止。
    真正保存由UE完成；冷重载、服务器GAS、Cook和联机验收另行执行，不由本函数宣称通过。
    """
    import copy
    import shutil
    folder = WORKSPACE / 'Saved/Validation/TargetRange50m20261021'
    check(Path(unreal.Paths.get_project_file_path()).resolve() == WORKSPACE/'Game/DivineBeastsArena.uproject', '正式工程身份不符')
    check(not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() and
          not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '存在未保存人工包，拒绝覆盖')
    plans, motions, attack_ids = [], [], set()
    roots = ((ROOT, 'DT_DBA_PlaceholderBalance', 72, 48),
             (ROOT+'/HotbarFallbacks', 'DT_DBA_PlayableHotbarFallbackBalance', 36, 24),
             ('/Game/Development/DivineBeasts/Abilities/MagicTests', 'DT_DBA_DevMagicBalance', 36, 36))
    for base, name, total, attacks in roots:
        table = unreal.load_asset(base+'/'+name)
        check(isinstance(table, unreal.DataTable), '数值表缺失 '+name)
        before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        after = copy.deepcopy(before)
        check(len(before) == total, '数值行数发生作者变化 '+name)
        count = 0
        for row in after:
            if row['BaseDamage'] <= 0: continue
            check(row['CastRangeCm'] in (2000, 5000), '已有另行设计的射程 '+row['Name'])
            key = row['Name'].removesuffix('_L1')
            if 'HotbarFallbacks' in base:
                hero, slot = key.split('_')
                path = base+'/GA_DBA_'+hero+'_Hotbar'+slot
            elif base.startswith('/Game/'):
                hero, spell = key.split('_')
                path = base+'/DA_DBA_'+hero+'_Dev'+spell
            else:
                hero, _, spell = key.split('_')
                path = base+'/GA_DBA_'+hero+'_Placeholder'+spell
            obj = unreal.load_asset(path)
            check(obj is not None, '运动资产缺失 '+path)
            is_bp = isinstance(obj, unreal.Blueprint)
            value = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path)) if is_bp else obj
            motion = value.get_editor_property('ProjectileMotion')
            check(motion.get_editor_property('RangeCm') == row['CastRangeCm'], '数值和运动范围不一致 '+path)
            motions.append((path, obj, value, motion, is_bp))
            attack_ids.add(row['AbilityId']);row['CastRangeCm'] = 5000;count += 1
        check(count == attacks, '攻击行分类发生变化 '+name)
        plans.append((table, before, after))
    check(len(motions) == 108 and len(attack_ids) == 108, '攻击资产身份重复或不完整')
    # UE挂载点转物理目录仅用于原包备份；禁止用文件工具制作或修改uasset。
    for obj in [p[0] for p in plans]+[m[1] for m in motions]:
        package = obj.get_path_name().split('.')[0]
        relative = ('Game/Content/'+package[len('/Game/'):]+'.uasset') if package.startswith('/Game/') else (
            'Game/Plugins/DivineBeasts/DBAGameplay/Content/'+package[len('/DBAGameplay/'):]+'.uasset')
        source = WORKSPACE/relative
        check(source.is_file(), '真实包备份源缺失 '+relative)
        dest = folder/'Backup'/relative
        check(not dest.exists(), '本轮备份已存在，先检查前次迁移结果')
        dest.parent.mkdir(parents=True, exist_ok=True);shutil.copy2(source, dest)
    (folder/'RangeAssets.before.json').write_text(json.dumps({t.get_path_name():b for t,b,a in plans},ensure_ascii=False,indent=2),encoding='utf-8')
    for table, before, after in plans:
        check(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(after,ensure_ascii=False)), 'UE填表失败')
        save(table)
    for path, obj, value, motion, is_bp in motions:
        motion.set_editor_property('RangeCm',5000.)
        value.set_editor_property('ProjectileMotion',motion)
        if is_bp: unreal.BlueprintEditorLibrary.compile_blueprint(obj)
        save(obj)
        actual = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path)) if is_bp else obj
        check(actual.get_editor_property('ProjectileMotion').get_editor_property('RangeCm') == 5000, '编译后范围不符 '+path)
    result = {'Saved':True,'AttackRows':108,'AttackMotionAssets':108,'MaxTargetRangeCm':5000,
              'PreservedHealingRangeCm':2000,'HumanAccepted':False,'ColdReloadVerified':False,
              'MotionPaths':[m[0] for m in motions]}
    (folder/'RangeAssets.saved.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('SSLM_ATTACK_RANGE_SAVED_50M',len(motions))
    return result
