# -*- coding: utf-8 -*-
"""
SSLM十二生肖占位技能原生资产作者：通过UE5.8 Editor Python生成真实资产，不制作文本伪uasset。
产出：12英雄 × (5份技能定义 + 5份独立GAS蓝图 + 1份授权集 + 1份客户端UIProfile)
     +36成本GE+36冷却GE + 1份60行数值表。
默认技能仅使用普通攻击、无效果被动、火焰弹、寒冰箭、奥术飞弹。
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
    ("Primary", "普通攻击（占位）", "PRIMARY", "PRIMARY", 40, 0, 0, 250, 0, 0, 1, 0),
    ("Passive", "被动技能（占位）", "PASSIVE", "", 0, 0, 0, 0, 0, 0, 1, 0),
    ("Fireball", "火焰弹（占位）", "ACTIVE", "SLOT1", 40, 10, 2, 2000, 1200, 0, 1, 0),
    ("Frostbolt", "寒冰箭（占位）", "ACTIVE", "SLOT2", 30, 8, 1.5, 2000, 1600, 0, 1, 0),
    ("ArcaneMissiles", "奥术飞弹（占位）", "ULTIMATE", "SLOT4", 12, 12, 3, 2000, 1400, 18, 3, .2),
)
COOLDOWNS = ("Fireball", "Frostbolt", "ArcaneMissiles")
MAGIC_ICONS = {
    name: "/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev" + name
    for name in COOLDOWNS
}
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
    for input_name in ("Primary", "Slot1", "Slot2", "Slot4"):
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
                "BaseDamage": damage, "DamageType": "Physical" if mode == "Primary" else
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
            if cost > 0:
                cost_bp, _temp, cost_cdo = blueprint(ROOT + "/GE" + base_name + "Cost", unreal.GameplayEffect)
                cost_cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.INSTANT)
                attr = ('(Attribute="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet:Momentum",'
                        'AttributeOwner="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet",'
                        'AttributeName="Momentum")')
                mod = unreal.GameplayModifierInfo()
                check(mod.import_text("(Attribute=" + attr +
                    ",ModifierOp=AddBase,ModifierMagnitude=" + magnitude(-cost).export_text() + ")"),
                    "真实气势GAS成本设置失败：" + identity)
                cost_cdo.set_editor_property("Modifiers", [mod])
                unreal.BlueprintEditorLibrary.compile_blueprint(cost_bp)
                save(cost_bp)
                cost_cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + base_name + "Cost")
                check(cost_cls is not None, "成本效果无真实生成类：" + identity)
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
                getattr(unreal.DivineBeastsPlaceholderAbilityMode, mode.upper() if mode != "ArcaneMissiles"
                    else "ARCANE_MISSILES"))
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
                                    ["Ability.Active"] if mode == "Primary"
                                    else ["Ability.Active", "Ability.Spell"]))
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
                description = "临时普通攻击：服务器前向单体基础伤害40，未实现该生肖真实战斗动作。"
            else:
                description = (
                    "临时" + label + "：每枚基础伤害" + str(damage) +
                    "，气势消耗" + str(cost) + "，冷却" + str(cooldown) +
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
        "Project": "SSLM", "HeroCount": len(heroes), "AbilityDefinitionCount": 60,
        "GameplayAbilityBlueprintCount": 60, "PlaceholderAbilitySetCount": 12,
        "ClientUIProfileCount": 12, "CostEffectCount": 36,
        "CooldownEffectCount": 36, "BalanceRowCount": 60,
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


if __name__ == "__main__":
    try:
        author()
    except Exception as error:
        unreal.log_error("SSLM_PLACEHOLDER_AUTHORING_FAILED=" + str(error))
        raise
