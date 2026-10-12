# -*- coding: utf-8 -*-
"""通过Monolith在正式UE编辑器增量制作开发魔法；不会直接写uasset或重做正式英雄。
三种参数为可替换的测试平衡。只替换12个已有开发集合/Profile的Active01/Active02/Ultimate，
保留原普攻、被动和旧技能资产。现有开发集合仍经非发行+配置+命令行三重启用。
调用前编辑器必须完成原生构建；备份旧25份实际资产到Saved，不删除源或原PlayerStart。
失败立即抛出，调用者需停止发布、保留证据；最后保存与独立重载/联机验证分开。
"""
import importlib.util
import json
import shutil
from pathlib import Path
from datetime import datetime, timezone

WORKSPACE = Path(__file__).resolve().parents[3]
ROOT = "/Game/Development/DivineBeasts/Abilities"
MAGIC = ROOT + "/MagicTests"
SPELLS = {
    "Fireball": {"Slot": "Active01", "NameZh": "火焰弹（开发测试）", "Damage": 40, "Cost": 10, "Cooldown": 2,
                 "Speed": 1200, "Radius": 0, "Turns": 0, "Count": 1, "Interval": 0},
    "Frostbolt": {"Slot": "Active02", "NameZh": "寒冰箭（开发测试）", "Damage": 30, "Cost": 8, "Cooldown": 1.5,
                  "Speed": 1600, "Radius": 0, "Turns": 0, "Count": 1, "Interval": 0},
    "ArcaneMissiles": {"Slot": "Ultimate", "NameZh": "螺旋奥术飞弹（开发测试）", "Damage": 12, "Cost": 12, "Cooldown": 3,
                       "Speed": 1400, "Radius": 18, "Turns": 3, "Count": 3, "Interval": 0.2},
}


def author():
    """受控引擎写入入口。先验证旧装配/图标存在，再备份；返回真实保存身份，不声称视觉通过。"""
    import unreal
    if "-MagicHUDOwnedAuthoring" not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError("不是本轮独占Monolith制作编辑器")
    if Path(unreal.Paths.get_project_file_path()).resolve() != WORKSPACE / "Game/DivineBeastsArena.uproject":
        raise RuntimeError("正式项目身份不符")
    spec = importlib.util.spec_from_file_location("zodiac_development_source", Path(__file__).with_name("AuthorZodiacDevelopmentAbilities.py"))
    source = importlib.util.module_from_spec(spec); spec.loader.exec_module(source)
    heroes = source.load_plan()  # 只读稳定英雄/槽位输入，不调用旧全量作者。
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    existing = {}
    for hero in heroes:
        ability_set = unreal.load_asset(ROOT + "/DA_DBA_" + hero + "_DevAbilitySet")
        profile = unreal.load_asset(ROOT + "/UI/Profiles/DA_DBA_" + hero + "_DevUIProfile")
        if not isinstance(ability_set, unreal.GamePlatformAbilitySetDefinition) or not isinstance(profile, unreal.DivineBeastsAbilityUIProfile):
            raise RuntimeError("原开发装配缺失，禁止另建重复来源: " + hero)
        if len(ability_set.get_editor_property("Abilities")) != 5 or len(profile.get_editor_property("Entries")) != 5:
            raise RuntimeError("旧开发五槽合同变化，禁止覆盖: " + hero)
        existing[hero] = (ability_set, profile)
    for name in SPELLS:
        if not isinstance(unreal.load_asset("/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev" + name), unreal.Texture2D):
            raise RuntimeError("原创魔法图标必须先通过Monolith导入: " + name)
    evidence = WORKSPACE / "Saved/Validation/MagicHUD20261012" / ("Author-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S"))
    backup = evidence / "Backup"; backup.mkdir(parents=True)
    for hero in heroes:
        for relative in ("DA_DBA_" + hero + "_DevAbilitySet.uasset", "UI/Profiles/DA_DBA_" + hero + "_DevUIProfile.uasset"):
            original = WORKSPACE / "Game/Content/Development/DivineBeasts/Abilities" / relative
            if not original.is_file(): raise RuntimeError("缺少可回退磁盘源: " + str(original))
            dest = backup / relative; dest.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(original, dest)
    saved = []

    def save(asset):
        """只由引擎序列化保存；失败不得标为已完成。"""
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("引擎保存失败: " + asset.get_path_name())
        saved.append(asset.get_path_name())

    def asset(path, cls, factory):
        """同名不同类型拒绝；幂等再运行仅复用本轮准确类型。"""
        obj = unreal.load_asset(path)
        if obj is None: obj = tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], cls, factory)
        if not isinstance(obj, cls): raise RuntimeError("创建/类型错误: " + path)
        return obj

    def logical(identifier):
        """保持平台逻辑身份结构，不从资源路径充当ID。"""
        namespace, rest = identifier.rsplit(".", 1); name, version = rest.rsplit("@", 1)
        value = unreal.GamePlatformId(); value.set_editor_property("Namespace", namespace)
        value.set_editor_property("Name", name); value.set_editor_property("LogicalVersion", int(version)); return value

    def primary(identifier):
        """定义ID用于原授予事务Gameplay预热依赖，不创建第二个资产管理器。"""
        value = unreal.PrimaryAssetId(); kind = unreal.PrimaryAssetType(); kind.set_editor_property("Name", "GamePlatformDefinition")
        value.set_editor_property("PrimaryAssetType", kind); value.set_editor_property("PrimaryAssetName", identifier); return value

    def tags(names):
        """真实字典资格检查，不把ImportText成功当成网络标签已登记。"""
        value = unreal.GameplayTagContainer()
        for name in names:
            tag = unreal.GameplayTag(); tag.import_text(name)
            if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag): raise RuntimeError("未登记标签: " + name)
        if not value.import_text('(GameplayTags=(' + ','.join('(TagName="' + n + '")' for n in names) + '))'):
            raise RuntimeError("标签容器导入失败")
        return value

    def blueprint(path, parent):
        """原生派生无伤害蓝图图表；资源只提供静态成本/冷却和真实定义身份。"""
        factory = unreal.BlueprintFactory(); factory.set_editor_property("ParentClass", parent)
        bp = asset(path, unreal.Blueprint, factory); unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if not cls or not isinstance(unreal.get_default_object(cls), parent): raise RuntimeError("父类不符: " + path)
        return bp, cls, unreal.get_default_object(cls)

    def magnitude(number):
        """原生GAS静态幅值；不自行计算气势或维护冷却计时器。"""
        value = unreal.GameplayEffectModifierMagnitude()
        if not value.import_text('(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=' + str(float(number)) + '))'):
            raise RuntimeError("GAS幅值导入失败")
        return value

    factory = unreal.DataTableFactory(); factory.set_editor_property("Struct", unreal.load_object(None, "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow"))
    table = asset(MAGIC + "/DT_DBA_DevMagicBalance", unreal.DataTable, factory)
    rows = []
    for hero in heroes:
        for name, data in SPELLS.items():
            identifier = "dba.ability." + hero.lower() + "_dev_" + name.lower() + "@1"
            rows.append({"Name": hero + "_" + name + "_L1", "AbilityId": identifier, "Level": 1, "BaseDamage": data["Damage"],
                         "DamageType": "Magic", "CooldownSeconds": data["Cooldown"], "MomentumCost": data["Cost"], "CastRangeCm": 2000, "AreaRadiusCm": 0})
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)): raise RuntimeError("魔法数值表导入失败")
    save(table)
    for hero, entries in heroes.items():
        ability_set, profile = existing[hero]
        grants = list(ability_set.get_editor_property("Abilities")); dependencies = list(ability_set.get_editor_property("RequiredDefinitions"))
        ui = list(profile.get_editor_property("Entries"))
        for name, data in SPELLS.items():
            index = source.SLOTS.index(data["Slot"]); identifier = "dba.ability." + hero.lower() + "_dev_" + name.lower() + "@1"
            old_id = entries[index]["DevelopmentAbilityId"]
            # 再运行仅接受原候选或本轮准确身份；用户修改成其他技能时停止，不覆盖人工工作。
            if str(grants[index].get_editor_property("AbilityId")) not in (old_id, identifier):
                # 结构字符串不是稳定ID视图，使用export_text内字段组合独立解析。
                old_value = grants[index].get_editor_property("AbilityId")
                actual = str(old_value.get_editor_property("Namespace")) + "." + str(old_value.get_editor_property("Name")) + "@" + str(old_value.get_editor_property("LogicalVersion"))
                if actual not in (old_id, identifier): raise RuntimeError("开发槽位已有人工替换: " + actual)
            prefix = "_DBA_" + hero + "_Dev" + name
            data_factory = unreal.DataAssetFactory(); data_factory.set_editor_property("DataAssetClass", unreal.DivineBeastsDevelopmentMagicDefinition)
            definition = asset(MAGIC + "/DA" + prefix, unreal.DivineBeastsDevelopmentMagicDefinition, data_factory)
            definition.set_editor_property("LogicalId", logical(identifier)); definition.set_editor_property("HeroDefinitionId", "Hero.Zodiac." + hero)
            definition.set_editor_property("Kind", unreal.DivineBeastsAbilityKind.ULTIMATE if data["Slot"] == "Ultimate" else unreal.DivineBeastsAbilityKind.ACTIVE)
            definition.set_editor_property("BalanceTable", table); definition.set_editor_property("LevelRowNames", [hero + "_" + name + "_L1"])
            definition.set_editor_property("MagicKind", getattr(unreal.DivineBeastsDevelopmentMagicKind, {"Fireball": "FIREBALL", "Frostbolt": "FROSTBOLT", "ArcaneMissiles": "ARCANE_MISSILES"}[name]))
            motion = unreal.GamePlatformCombatProjectileMotion()
            for field, number in {"SpeedCmPerSecond": data["Speed"], "RangeCm": 2000, "CollisionRadiusCm": 8,
                                  "HelixRadiusCm": data["Radius"], "HelixTurnsPerSecond": data["Turns"], "InitialPhaseTurns": 0}.items():
                motion.set_editor_property(field, number)
            definition.set_editor_property("ProjectileMotion", motion); definition.set_editor_property("ProjectileCount", data["Count"])
            definition.set_editor_property("ProjectileIntervalSeconds", data["Interval"]); save(definition)
            cost_bp, cost_cls, cost = blueprint(MAGIC + "/GE" + prefix + "Cost", unreal.GameplayEffect)
            cost.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.INSTANT)
            modifier = unreal.GameplayModifierInfo()
            attr = '(Attribute="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet:Momentum",AttributeOwner="/Script/DivineBeastsCharactersRuntime.DivineBeastsMomentumAttributeSet",AttributeName="Momentum")'
            if not modifier.import_text('(Attribute=' + attr + ',ModifierOp=AddBase,ModifierMagnitude=' + magnitude(-data["Cost"]).export_text() + ')'): raise RuntimeError("成本导入失败")
            cost.set_editor_property("Modifiers", [modifier]); unreal.BlueprintEditorLibrary.compile_blueprint(cost_bp); save(cost_bp)
            cool_bp, cool_cls, cool = blueprint(MAGIC + "/GE" + prefix + "Cooldown", unreal.GameplayEffect)
            cool.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.HAS_DURATION)
            cool.set_editor_property("DurationMagnitude", magnitude(data["Cooldown"])); unreal.BlueprintEditorLibrary.compile_blueprint(cool_bp); save(cool_bp)
            bp, cls, cdo = blueprint(MAGIC + "/GA" + prefix, unreal.DivineBeastsDevelopmentMagicAbility)
            cdo.set_editor_property("AbilityDefinitionId", primary(identifier)); cdo.set_editor_property("CostGameplayEffectClass", cost_cls)
            cdo.set_editor_property("CooldownGameplayEffectClass", cool_cls); cdo.set_editor_property("bDevelopmentDamageSampleSupported", True)
            cdo.set_editor_property("DevelopmentCooldownTags", tags(["DivineBeasts.Development.Cooldown." + hero + "." + data["Slot"]]))
            cdo.set_editor_property("AbilityTags", tags(["Ability.Active", "Ability.Spell"]))
            unreal.BlueprintEditorLibrary.compile_blueprint(bp); save(bp)
            grant = unreal.GamePlatformAbilityGrant(); input_tag = unreal.GameplayTag(); input_tag.import_text(entries[index]["InputTag"])
            if not grant.import_text('(AbilityId=' + logical(identifier).export_text() + ',AbilityClass="' + cls.get_path_name() + '",AbilityLevel=1,InputTag=' + input_tag.export_text() + ')'): raise RuntimeError("魔法授权导入失败")
            grants[index] = grant
            dependencies = [d for d in dependencies if str(d.get_editor_property("PrimaryAssetName")) not in (old_id, identifier)] + [primary(identifier)]
            item = unreal.DivineBeastsAbilityUIEntry(); item.set_editor_property("AbilityId", identifier); item.set_editor_property("DisplayName", unreal.Text(data["NameZh"]))
            item.set_editor_property("Description", unreal.Text("开发伤害测试：每枚基础法术伤害" + str(data["Damage"]) + "，气势" + str(data["Cost"]) + "，冷却" + str(data["Cooldown"]) + "秒。" + ("三枚实际螺旋飞行，间隔0.2秒；取消停止余弹。" if data["Count"] == 3 else "服务器直线投射；未实现燃烧、冰冻或减速。") + "后续替换正式技能。"))
            item.set_editor_property("Icon", unreal.load_asset("/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev" + name)); ui[index] = item
        ability_set.set_editor_property("Abilities", grants); ability_set.set_editor_property("RequiredDefinitions", dependencies)
        profile.set_editor_property("Entries", ui); save(ability_set); save(profile)
    report = {"Saved": saved, "Backup": str(backup), "FormalHeroAssetsModified": False, "ShippingEligible": False,
              "LimitationsZh": "开发单目标投射物伤害；非正式平衡，不实现燃烧/冰冻/减速/追踪。编译保存不代表联网和人工VFX验收。"}
    (evidence / "Report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"SavedWrites": len(saved), "Report": str(evidence / "Report.json")}, ensure_ascii=False))
    return report
