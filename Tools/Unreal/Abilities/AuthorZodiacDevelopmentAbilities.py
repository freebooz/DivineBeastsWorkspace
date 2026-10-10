# -*- coding: utf-8 -*-
"""
生肖开发技能作者脚本，由主任务通过Monolith在锁定UE5.8编辑器调用。
输入是现有候选草案，输出真实引擎DataAsset、DataTable、GE及唯一技能蓝图；不写二进制占位。
调用方必须先编译DivineBeastsDevelopmentGameplayAbility，并显式调用author()；导入本文件无资产副作用。
全部技能仍是开发身份。只实现前向单目标基础伤害，未实现机制由原生GAS资格检查拒绝。
编辑器修改串行执行；失败立即抛错并保留已完成资产供审计/恢复，调用者完成后需独立重载核验。
"""
import json
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
DRAFT_PATH = WORKSPACE / "Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json"
ROOT = "/Game/Development/DivineBeasts/Abilities"
UI_ROOT = ROOT + "/UI/Profiles"
SLOTS = ("BasicAttack", "Passive", "Active01", "Active02", "Ultimate")


def load_plan():
    """读取并校验候选，拒绝重复身份和缺失槽位；保留正式身份未批准边界。"""
    source = json.loads(DRAFT_PATH.read_text(encoding="utf-8-sig"))
    entries = source["Entries"]
    if len(entries) != 60 or source["ShippingEligible"]:
        raise ValueError("候选必须为60项且禁止发行")
    ids = set()
    heroes = {}
    for entry in entries:
        hero = entry["HeroDefinitionId"].removeprefix("Hero.Zodiac.")
        slot = entry["Slot"]
        # 草案既有身份只对子鼠普攻使用primary，其余生肖保留basicattack，不能为了表面一致迁移ID。
        expected = "dba.ability." + hero.lower() + "_dev_" + ("primary" if hero == "Rat" and slot == "BasicAttack" else slot.lower()) + "@1"
        if entry["DevelopmentAbilityId"] != expected or expected in ids or slot not in SLOTS:
            raise ValueError("候选开发身份冲突: " + expected)
        ids.add(expected)
        heroes.setdefault(hero, []).append(entry)
    if len(heroes) != 12 or any([entry["Slot"] for entry in group] != list(SLOTS) for group in heroes.values()):
        raise ValueError("十二英雄必须各自具有顺序一致的五槽")
    return heroes


def is_damage_sample(entry):
    """只认真实正数基础伤害和合法射线距离；不把美术主题/范围/系数解释为已实现玩法。"""
    balance = entry["Level1BalanceDraft"]
    return entry["Slot"] != "Passive" and balance["BaseDamage"] > 0 and balance["CastRangeCm"] > 0


def author(update_ui_profiles=True):
    """显式资产写入入口；UIProfile修改也由调用本脚本的Monolith编辑器操作执行。"""
    import unreal
    heroes = load_plan()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    saved = []

    def get_asset(path, cls, factory):
        """幂等复用已有对象；类型不一致拒绝，不能覆盖用户同名不同类型资产。"""
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            asset = tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], cls, factory)
        if asset is None or not isinstance(asset, cls):
            raise RuntimeError("资产创建或类型校验失败: " + path)
        return asset

    def save(asset):
        """检查引擎保存结果；不以内存对象存在冒充磁盘交付。"""
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("UE资产保存失败: " + asset.get_path_name())
        saved.append(asset.get_path_name())

    def logical(identifier):
        """通过已有逻辑身份结构导入，禁止以路径代替稳定编号。"""
        namespace, rest = identifier.rsplit(".", 1)
        name, version = rest.rsplit("@", 1)
        value = unreal.GamePlatformId()
        value.set_editor_property("Namespace", namespace)
        value.set_editor_property("Name", name)
        value.set_editor_property("LogicalVersion", int(version))
        return value

    def primary(identifier):
        """构造统一数据服务可消费的主资产ID，依赖与根定义共用租约。"""
        asset_type = unreal.PrimaryAssetType()
        asset_type.set_editor_property("Name", "GamePlatformDefinition")
        value = unreal.PrimaryAssetId()
        value.set_editor_property("PrimaryAssetType", asset_type)
        value.set_editor_property("PrimaryAssetName", identifier)
        return value

    def tags(tag_name):
        """仅使用已在配置中注册的标签；空串表示无冷却或未实现输入。"""
        value = unreal.GameplayTagContainer()
        if tag_name:
            tag(tag_name)  # 冷却标签同样须核对字典，不能仅把可解析文本当作有效网络标签。
        if tag_name and not value.import_text('(GameplayTags=((TagName="' + tag_name + '")))'):
            raise RuntimeError("标签容器导入失败: " + tag_name)
        return value

    def tag(tag_name):
        value = unreal.GameplayTag()
        if tag_name and not value.import_text(tag_name):
            raise RuntimeError("输入标签导入失败: " + tag_name)
        if tag_name and not unreal.GameplayTagLibrary.is_gameplay_tag_valid(value):
            raise RuntimeError("输入标签尚未登记: " + tag_name)
        return value

    def blueprint(path, parent):
        """生成无伪造图表的原生派生Blueprint；原生开发类提供真实生命周期和伤害逻辑。"""
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("ParentClass", parent)
        asset = get_asset(path, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if cls is None:
            raise RuntimeError("蓝图没有可用生成类: " + path)
        cdo = unreal.get_default_object(cls)
        if not isinstance(cdo, parent):
            raise RuntimeError("已有同名蓝图父类不符合作者要求，拒绝覆盖: " + path)
        return asset, cls, cdo

    def magnitude(number):
        """GE静态可求值幅值，与装配时成本/冷却门禁保持相同一级数值。"""
        value = unreal.GameplayEffectModifierMagnitude()
        # EditDefaultsOnly结构字段在Python不可直接写，使用UE原生结构导入而非绕过只读反射。
        if not value.import_text('(MagnitudeCalculationType=ScalableFloat,ScalableFloatMagnitude=(Value=' + str(float(number)) + '))'):
            raise RuntimeError("GE幅值结构导入失败")
        return value

    balance_factory = unreal.DataTableFactory()
    balance_factory.set_editor_property("Struct", unreal.load_object(None, "/Script/DivineBeastsAbilitiesRuntime.DivineBeastsAbilityBalanceRow"))
    table = get_asset(ROOT + "/DT_DBA_Zodiac_DevBalance", unreal.DataTable, balance_factory)
    rows = []
    balance_fields = ("AbilityId", "Level", "BaseDamage", "DamageType", "CooldownSeconds", "MomentumCost", "CastRangeCm", "AreaRadiusCm")
    for hero, entries in heroes.items():
        for entry in entries:
            row = {key: entry["Level1BalanceDraft"][key] for key in balance_fields}
            row["Name"] = hero + "_" + entry["Slot"] + "_L1"
            rows.append(row)
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
        raise RuntimeError("真实UE数值表导入失败")
    save(table)
    count_damage = 0
    unsupported = []
    for hero, entries in heroes.items():
        grants, dependencies, ui_entries = [], [], []
        for entry in entries:
            slot = entry["Slot"]
            tail = "Primary" if hero == "Rat" and slot == "BasicAttack" else slot
            prefix = "_DBA_" + hero + "_Dev" + tail
            data_factory = unreal.DataAssetFactory()
            data_factory.set_editor_property("DataAssetClass", unreal.DivineBeastsAbilityDefinition)
            definition = get_asset(ROOT + "/DA" + prefix, unreal.DivineBeastsAbilityDefinition, data_factory)
            identifier = entry["DevelopmentAbilityId"]
            definition.set_editor_property("LogicalId", logical(identifier))
            definition.set_editor_property("HeroDefinitionId", unreal.Name(entry["HeroDefinitionId"]))
            kind = "PRIMARY" if slot == "BasicAttack" else "PASSIVE" if slot == "Passive" else "ULTIMATE" if slot == "Ultimate" else "ACTIVE"
            definition.set_editor_property("Kind", getattr(unreal.DivineBeastsAbilityKind, kind))
            definition.set_editor_property("BalanceTable", table)
            definition.set_editor_property("LevelRowNames", [unreal.Name(hero + "_" + slot + "_L1")])
            save(definition)
            ability_path = ROOT + "/GA" + prefix
            # 旧子鼠样板包含仅Commit/End的图表，不复用它作为伤害完成证据；保留旧资产改用新唯一开发实现。
            if hero == "Rat" and slot == "BasicAttack":
                ability_path += "Native"
            bp, cls, cdo = blueprint(ability_path, unreal.DivineBeastsDevelopmentGameplayAbility)
            cdo.set_editor_property("AbilityDefinitionId", primary(identifier))
            balance = entry["Level1BalanceDraft"]
            cost_class = None
            cooldown_class = None
            if balance["MomentumCost"] > 0:
                cost_bp, cost_class, cost_cdo = blueprint(ROOT + "/GE" + prefix + "Cost", unreal.GameplayEffect)
                cost_cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.INSTANT)
                attr = unreal.GameplayAttribute()
                attr_text = '(Attribute="/Script/DivineBeastsAbilitiesRuntime.DivineBeastsMomentumAttributeSet:Momentum",AttributeOwner="/Script/DivineBeastsAbilitiesRuntime.DivineBeastsMomentumAttributeSet",AttributeName="Momentum")'
                if not attr.import_text(attr_text):
                    raise RuntimeError("气势属性真实FieldPath导入失败")
                modifier = unreal.GameplayModifierInfo()
                modifier.set_editor_property("Attribute", attr)
                modifier.set_editor_property("ModifierOp", unreal.GameplayModOp.ADD_BASE)
                modifier.set_editor_property("ModifierMagnitude", magnitude(-balance["MomentumCost"]))
                cost_cdo.set_editor_property("Modifiers", [modifier])
                unreal.BlueprintEditorLibrary.compile_blueprint(cost_bp)
                save(cost_bp)
                cost_class = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + prefix + "Cost")
            cooldown_tag = ""
            if balance["CooldownSeconds"] > 0:
                cool_bp, cooldown_class, cool_cdo = blueprint(ROOT + "/GE" + prefix + "Cooldown", unreal.GameplayEffect)
                cool_cdo.set_editor_property("DurationPolicy", unreal.GameplayEffectDurationType.HAS_DURATION)
                cool_cdo.set_editor_property("DurationMagnitude", magnitude(balance["CooldownSeconds"]))
                unreal.BlueprintEditorLibrary.compile_blueprint(cool_bp)
                save(cool_bp)
                cooldown_class = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GE" + prefix + "Cooldown")
                cooldown_tag = "DivineBeasts.Development.Cooldown." + hero + "." + slot
            cdo.set_editor_property("CostGameplayEffectClass", cost_class)
            cdo.set_editor_property("CooldownGameplayEffectClass", cooldown_class)
            cdo.set_editor_property("DevelopmentCooldownTags", tags(cooldown_tag))
            cdo.set_editor_property("bDevelopmentDamageSampleSupported", is_damage_sample(entry))
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            save(bp)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(ability_path)
            active = is_damage_sample(entry)
            count_damage += int(active)
            if not active:
                unsupported.append(identifier)
            grant = unreal.GamePlatformAbilityGrant()
            input_tag = tag(entry["InputTag"] or "")
            grant_text = '(AbilityId=' + logical(identifier).export_text() + ',AbilityClass="' + cls.get_path_name() + '",AbilityLevel=1,InputTag=' + input_tag.export_text() + ')'
            if not grant.import_text(grant_text):
                raise RuntimeError("能力授权条目导入失败: " + identifier)
            grants.append(grant)
            dependencies.append(primary(identifier))
            if update_ui_profiles:
                item = unreal.DivineBeastsAbilityUIEntry()
                item.set_editor_property("AbilityId", unreal.Name(identifier))
                item.set_editor_property("DisplayName", unreal.Text(entry["DisplayNameZh"]))
                desc = "开发验证：服务器前向单目标基础伤害；图示特殊机制、范围、伤害系数及暴击尚未实现。" if active else "开发验证：此候选的被动/辅助/位移机制尚未实现，GAS激活检查拒绝释放。"
                item.set_editor_property("Description", unreal.Text(desc))
                icon = unreal.EditorAssetLibrary.load_asset(entry["IconAssetPath"])
                if not isinstance(icon, unreal.Texture2D):
                    raise RuntimeError("候选真实图标未找到: " + entry["IconAssetPath"])
                item.set_editor_property("Icon", icon)
                ui_entries.append(item)
        set_factory = unreal.DataAssetFactory()
        set_factory.set_editor_property("DataAssetClass", unreal.GamePlatformAbilitySetDefinition)
        ability_set = get_asset(ROOT + "/DA_DBA_" + hero + "_DevAbilitySet", unreal.GamePlatformAbilitySetDefinition, set_factory)
        ability_set.set_editor_property("LogicalId", logical("dba.abilityset." + hero.lower() + "_dev@1"))
        ability_set.set_editor_property("bDevelopmentOnly", True)
        ability_set.set_editor_property("Abilities", grants)
        ability_set.set_editor_property("RequiredDefinitions", dependencies)
        ability_set.set_editor_property("Effects", [])
        ability_set.set_editor_property("Attributes", [])
        save(ability_set)
        if update_ui_profiles:
            profile_name = "DA_DBA_" + hero + "_DevUIProfile"
            old = ROOT + "/" + profile_name
            new = UI_ROOT + "/" + profile_name
            if unreal.EditorAssetLibrary.does_asset_exist(old) and not unreal.EditorAssetLibrary.does_asset_exist(new):
                referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(old, load_assets_to_confirm=True)
                if any(not str(reference).startswith(ROOT + "/") for reference in referencers):
                    raise RuntimeError("开发UIProfile已有开发目录外引用，必须先审核迁移: " + old)
                if not unreal.EditorAssetLibrary.rename_asset(old, new):
                    raise RuntimeError("客户端表现配置目录隔离失败: " + old)
            profile_factory = unreal.DataAssetFactory()
            profile_factory.set_editor_property("DataAssetClass", unreal.DivineBeastsAbilityUIProfile)
            profile = get_asset(new, unreal.DivineBeastsAbilityUIProfile, profile_factory)
            profile.set_editor_property("HeroDefinitionId", unreal.Name("Hero.Zodiac." + hero))
            profile.set_editor_property("Entries", ui_entries)
            save(profile)
    report = {"SavedAssetWrites": len(saved), "Definitions": 60, "AbilitySets": 12,
              "GenericDamageSamples": count_damage, "DisabledCandidates": unsupported,
              "ShippingEligible": False, "FormalHeroAssetsModified": False,
              "MechanismLimitationsZh": "仅一级基础伤害和前向单目标射线；范围半径、攻击/法术系数、暴击、位移、治疗、护盾、被动规则均未实现。"}
    print("DBA_DEVELOPMENT_AUTHORING=" + json.dumps(report, ensure_ascii=False))
    return report
