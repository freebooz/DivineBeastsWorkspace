# -*- coding: utf-8 -*-
"""
十二生肖开发技能 UE5.8 真资产只读验证。
必须通过 Monolith MCP（虚幻编辑器工具）在正式 DivineBeastsArena Editor 内运行。

验证范围：12个Hero×5个Development AbilityDefinition、60行DataTable、
12份Development AbilityUIProfile、60张真实Texture2D。
发布资格另由 ValidateZodiacAbilityDelivery.py 的 --release 门禁决定；
本脚本的成功不能替代UE三端构建、GAS授予、伤害、PIE、Cook或正式策划批准。

执行入口：
Monolith editor.run_python(mode="execute_file",
 command="E:/poject/feebooz/DivineBeastsWorkspace/Tools/Unreal/Abilities/ValidateZodiacDevelopmentAssets.py")
"""
import re
import unreal

ROOT = "/Game/Development/DivineBeasts/Abilities/"
HEROES = (
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar",
)
SLOTS = ("BasicAttack", "Passive", "Active01", "Active02", "Ultimate")


def check(condition, message, errors):
    """累计错误以便一次显示全部阻断；不改变任何UObject或资产文件。"""
    if not condition:
        errors.append(message)


def validate():
    errors = []
    identities = set()
    ui_identities = set()
    icon_paths = set()
    ability_count = 0
    ui_profile_count = 0
    ui_entry_count = 0

    balance_table = unreal.EditorAssetLibrary.load_asset(
        ROOT + "DT_DBA_Zodiac_DevBalance")
    check(balance_table is not None, "统一开发技能DataTable不存在", errors)
    if balance_table is not None:
        row_names = {str(x) for x in
                     unreal.DataTableFunctionLibrary.get_data_table_row_names(
                         balance_table)}
    else:
        row_names = set()

    for hero in HEROES:
        profile_path = ROOT + "DA_DBA_" + hero + "_DevUIProfile"
        profile = unreal.EditorAssetLibrary.load_asset(profile_path)
        check(profile is not None, hero + ":技能界面开发资产不存在", errors)

        if profile is not None:
            ui_profile_count += 1
            check(
                profile.get_class().get_name() == "DivineBeastsAbilityUIProfile",
                hero + ":客户端配置资产的UClass类型错误", errors)
            check(str(profile.get_editor_property("HeroDefinitionId")) ==
                  "Hero.Zodiac." + hero,
                  hero + ":客户端配置对应的英雄编号错误", errors)
            entries = profile.get_editor_property("Entries")
            check(len(entries) == len(SLOTS),
                  hero + ":客户端技能配置不是5个槽位", errors)
            for entry_index, entry in enumerate(entries):
                ui_entry_count += 1
                entry_text = str(entry)
                icon_match = re.search(r'icon: "([^"]+)"', entry_text)
                icon_path = icon_match.group(1) if icon_match else ""
                check(bool(icon_path), hero + ":技能缺少纹理软引用", errors)
                if icon_path:
                    icon_paths.add(icon_path)
                    check(
                        unreal.EditorAssetLibrary.does_asset_exist(icon_path),
                        hero + ":客户端图标真实UE资产不存在: " + icon_path,
                        errors)
                development_id = str(entry.get_editor_property("AbilityId"))
                # 界面五槽不是独立技能真源：每个图标必须与同一英雄/槽位的技能定义使用完全相同的ID。
                # 只在开发目录内检查测试身份，绝不自动签发正式技能编号。
                if entry_index < len(SLOTS):
                    slot = SLOTS[entry_index]
                    name = ("rat_dev_primary" if hero == "Rat" and slot == "BasicAttack"
                            else hero.lower() + "_dev_" + slot.lower())
                    expected_id = "dba.ability." + name + "@1"
                    check(development_id == expected_id,
                          hero + "/" + slot + ":界面技能编号不等于玩法Definition: " +
                          development_id, errors)
                check(development_id not in ui_identities,
                      hero + ":不同图标复用同一技能编号: " + development_id,
                      errors)
                ui_identities.add(development_id)

        for slot in SLOTS:
            tail = "Primary" if hero == "Rat" and slot == "BasicAttack" else slot
            asset_path = ROOT + "DA_DBA_" + hero + "_Dev" + tail
            ability = unreal.EditorAssetLibrary.load_asset(asset_path)
            check(ability is not None, hero + "/" + slot +
                  ":实际AbilityDefinition不存在", errors)
            if ability is None:
                continue
            ability_count += 1
            check(
                ability.get_class().get_name() == "DivineBeastsAbilityDefinition",
                hero + "/" + slot + ":玩法主数据UClass类型错误", errors)
            check(
                str(ability.get_editor_property("HeroDefinitionId")) ==
                "Hero.Zodiac." + hero,
                hero + "/" + slot + ":定义所属英雄错误", errors)
            # 数值行为不得冒充具体技能类型；主动与被动互换应被真实引擎审计识别。
            expected_kind = ("PRIMARY" if slot == "BasicAttack" else
                             "PASSIVE" if slot == "Passive" else
                             "ULTIMATE" if slot == "Ultimate" else "ACTIVE")
            real_kind = str(ability.get_editor_property("Kind")).upper()
            check("." + expected_kind + ":" in real_kind,
                  hero + "/" + slot + ":技能类型与槽位不一致: " + real_kind,
                  errors)

            id_struct = ability.get_editor_property("LogicalId")
            namespace = str(id_struct.get_editor_property("Namespace"))
            name = str(id_struct.get_editor_property("Name"))
            version = id_struct.get_editor_property("LogicalVersion")
            identity = namespace + "." + name + "@" + str(version)
            expected_name = (
                "rat_dev_primary"
                if hero == "Rat" and slot == "BasicAttack"
                else hero.lower() + "_dev_" + slot.lower())
            expected_id = "dba.ability." + expected_name + "@1"
            check(identity == expected_id,
                  hero + "/" + slot + ":定义技能编号错误: " + identity, errors)
            check(identity not in identities,
                  hero + "/" + slot + ":技能编号重复: " + identity, errors)
            identities.add(identity)

            expected_row = hero + "_" + slot + "_L1"
            check(expected_row in row_names,
                  hero + "/" + slot + ":DataTable缺少等级数值行", errors)
            rows = ability.get_editor_property("LevelRowNames")
            check(
                len(rows) == 1 and str(rows[0]) == expected_row,
                hero + "/" + slot + ":定义配置的等级行名不匹配", errors)

    check(len(row_names) == 60, "统一开发技能数值行不是60条", errors)
    check(ui_profile_count == 12, "项目缺少开发技能界面配置", errors)
    check(ui_entry_count == 60, "技能界面开发条目不是60条", errors)
    check(len(icon_paths) == 60, "客户端软纹理身份不唯一", errors)
    check(len(identities) == 60, "逻辑技能身份不唯一", errors)
    check(len(ui_identities) == 60 and ui_identities == identities,
          "UI显示条目的全部技能ID与60份玩法Definition集合不一致", errors)

    ability_set = unreal.EditorAssetLibrary.load_asset(
        ROOT + "DA_DBA_Rat_DevAbilitySet")
    check(ability_set is not None, "子鼠开发技能授权集不存在", errors)
    if ability_set is not None:
        check(
            ability_set.get_class().get_name() ==
            "GamePlatformAbilitySetDefinition",
            "子鼠技能集真实类型错误", errors)
        check(
            ability_set.get_editor_property("bDevelopmentOnly") is True,
            "开发技能集未标注DevelopmentOnly", errors)

    blueprint = unreal.EditorAssetLibrary.load_asset(
        ROOT + "GA_DBA_Rat_DevPrimary")
    check(blueprint is not None, "子鼠开发GAS技能蓝图不存在", errors)
    if blueprint is not None:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(
            ROOT + "GA_DBA_Rat_DevPrimary")
        check(cls is not None, "子鼠开发技能蓝图没有有效生成类", errors)
        if cls is not None:
            cdo = unreal.get_default_object(cls)
            policy = str(cdo.get_editor_property("NetExecutionPolicy")).upper()
            check("SERVER_ONLY" in policy,
                  "子鼠开发技能未采用服务器独占执行策略", errors)
            identifier = cdo.get_editor_property("AbilityDefinitionId")
            check(
                str(identifier.get_editor_property("PrimaryAssetName")) ==
                "dba.ability.rat_dev_primary@1" and
                str(identifier.get_editor_property("PrimaryAssetType").get_editor_property("Name")) ==
                "GamePlatformDefinition",
                "子鼠技能类绑定的技能主资产与能力集不一致", errors)

    print("UE_DEVELOPMENT_ABILITY_DEFINITIONS=" + str(ability_count) + "/60")
    print("UE_DEVELOPMENT_BALANCE_ROWS=" + str(len(row_names)) + "/60")
    print("UE_DEVELOPMENT_UI_PROFILES=" + str(ui_profile_count) + "/12")
    print("UE_DEVELOPMENT_ICON_ENTRIES=" + str(ui_entry_count) + "/60")
    print("UE_DEVELOPMENT_ICON_ASSET_PATHS=" + str(len(icon_paths)) + "/60")
    print("UE_DEVELOPMENT_UI_IDS_MATCH_DEFINITIONS=" +
          str(len(ui_identities & identities)) + "/60")
    print("UE_DEVELOPMENT_RAT_EXECUTION_POLICY=SERVER_ONLY")
    print("DEVELOPMENT_ONLY=TRUE; FORMAL_RELEASE_NOT_VERIFIED")
    if errors:
        for problem in errors:
            print("FAILED_DEVELOPMENT_ASSET_CHECK: " + problem)
        raise RuntimeError("UE开发技能资源存在" + str(len(errors)) + "项错误")
    print("UE_DEVELOPMENT_ABILITY_ASSET_AUDIT=PASSED")


validate()
