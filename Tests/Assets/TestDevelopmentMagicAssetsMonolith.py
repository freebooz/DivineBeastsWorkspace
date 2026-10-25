# -*- coding: utf-8 -*-
"""Monolith真实资产只读门禁：36项开发魔法、12套授权/Profile和6套VFX，缺失明确失败。
预期数值为独立测试夹具，不调用作者生成函数求期望；不写资产，不把存在性称作网络/视觉验收。
运行方式：Monolith editor.run_python execute_file；必须打开正式UE5.8工程。
"""
from pathlib import Path
import json
import unreal

HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")
# 2026-10-24性能/观感调速：仅提高恒定轴向速度，保留50米射程、连发和螺旋合同。
EXPECTED = {"Fireball": (4000.0, 0.0, 0.0, 1, 0.0),
            "Frostbolt": (4000.0, 0.0, 0.0, 1, 0.0),
            "ArcaneMissiles": (4000.0, 18.0, 3.0, 3, 0.2)}
ROOT = "/Game/Development/DivineBeasts/Abilities/MagicTests"

def identifier(value):
    """独立还原稳定逻辑ID，不借用作者的生成函数或资产文件名作为正确性期望。"""
    return str(value.get_editor_property('Namespace'))+'.'+str(value.get_editor_property('Name'))+'@'+str(value.get_editor_property('LogicalVersion'))


def main():
    """全部收集错误后抛出；独占Saved报告不包含密码/凭据，也不覆盖既有导入报告。"""
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "DivineBeastsArena.uproject":
        raise RuntimeError("编辑器工程错误")
    errors, inspected = [], []
    # 用户2026-10-14明确法术不消耗气势；独立读取真实数值表，拒绝旧费用资产。
    table = unreal.load_asset(ROOT + '/DT_DBA_DevMagicBalance')
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)) if table else []
    if len(rows) != 36 or any(float(row['MomentumCost']) != 0 for row in rows):
        errors.append('36条真实法术数值必须全部为零气势费用')
    for hero in HEROES:
        base='/Game/Development/DivineBeasts/Abilities'
        ability_set=unreal.load_asset(base+'/DA_DBA_'+hero+'_DevAbilitySet')
        profile=unreal.load_asset(base+'/UI/Profiles/DA_DBA_'+hero+'_DevUIProfile')
        formal=unreal.load_asset('/DBAGameplay/Definitions/DA_Hero_Zodiac_'+hero)
        allowed = ('None', 'dba.abilityset.' + hero.lower() + '_placeholder@1')
        if not formal or str(formal.get_editor_property('DefaultAbilitySetId')) not in allowed:
            errors.append('正式英雄默认集合被开发技能或未知定义污染: ' + hero)
        if not isinstance(ability_set,unreal.GamePlatformAbilitySetDefinition) or not isinstance(profile,unreal.DivineBeastsAbilityUIProfile):
            errors.append('真实开发集合/Profile缺失: '+hero); continue
        grants=list(ability_set.get_editor_property('Abilities')); ui=list(profile.get_editor_property('Entries'))
        required={str(v.get_editor_property('PrimaryAssetName')) for v in ability_set.get_editor_property('RequiredDefinitions')}
        if len(grants)!=5 or len(ui)!=5 or len(required)!=5 or not ability_set.get_editor_property('bDevelopmentOnly'):
            errors.append('开发五槽/依赖闭包/开发标记不符: '+hero); continue
        for spell, expected in EXPECTED.items():
            path = ROOT + "/DA_DBA_" + hero + "_Dev" + spell
            asset = unreal.load_asset(path)
            inspected.append(path)
            if not asset or asset.get_class().get_name() != "DivineBeastsDevelopmentMagicDefinition":
                errors.append("缺少真实魔法定义: " + path)
                continue
            if str(asset.get_editor_property("HeroDefinitionId")) != "Hero.Zodiac." + hero:
                errors.append("英雄身份不符: " + path)
            wanted='dba.ability.'+hero.lower()+'_dev_'+spell.lower()+'@1'
            if identifier(asset.get_editor_property('LogicalId'))!=wanted: errors.append('定义逻辑ID不符: '+path)
            index={'Fireball':2,'Frostbolt':3,'ArcaneMissiles':4}[spell]
            slot={'Fireball':'Slot1','Frostbolt':'Slot2','ArcaneMissiles':'Slot4'}[spell]
            if identifier(grants[index].get_editor_property('AbilityId'))!=wanted or wanted not in required:
                errors.append('真实授权/预热依赖未替换: '+path)
            if grants[index].get_editor_property('InputTag').export_text()!='(TagName="Platform.Ability.Input.DivineBeasts.'+slot+'")':
                errors.append('技能输入槽变化: '+path)
            if str(ui[index].get_editor_property('AbilityId'))!=wanted or spell not in ui[index].get_editor_property('Icon').get_path_name():
                errors.append('实际UIProfile图标或技能ID错误: '+path)
            motion = asset.get_editor_property("ProjectileMotion")
            values = (float(motion.get_editor_property("SpeedCmPerSecond")),
                      float(motion.get_editor_property("HelixRadiusCm")),
                      float(motion.get_editor_property("HelixTurnsPerSecond")),
                      int(asset.get_editor_property("ProjectileCount")),
                      float(asset.get_editor_property("ProjectileIntervalSeconds")))
            if any(abs(a-b) > 0.0001 for a, b in zip(values, expected)):
                errors.append("运动/连发合同不符: " + path)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "/GA_DBA_" + hero + "_Dev" + spell)
            if cls is None or not isinstance(unreal.get_default_object(cls), unreal.DivineBeastsDevelopmentMagicAbility):
                errors.append("真实GAS派生/生成类错误: " + path)
            else:
                cdo=unreal.get_default_object(cls)
                if str(cdo.get_editor_property('AbilityDefinitionId').get_editor_property('PrimaryAssetName'))!=wanted:
                    errors.append('GAS DefinitionId不符: '+path)
                if cdo.get_editor_property('CostGameplayEffectClass') is not None:
                    errors.append('法术仍引用气势成本GE: '+path)
                legacy_class = unreal.EditorAssetLibrary.load_blueprint_class(ROOT+'/GE_DBA_'+hero+'_Dev'+spell+'Cost')
                if not legacy_class or unreal.get_default_object(legacy_class).get_editor_property('Modifiers'):
                    errors.append('保留身份的旧成本GE仍有资源修改: '+path)
                if '不消耗气势' not in str(ui[index].get_editor_property('Description')):
                    errors.append('法术界面说明仍含过期费用: '+path)
                for field,suffix in [('CooldownGameplayEffectClass','Cooldown')]:
                    effect=cdo.get_editor_property(field)
                    if not effect or effect.get_path_name()!=ROOT+'/GE_DBA_'+hero+'_Dev'+spell+suffix+'.GE_DBA_'+hero+'_Dev'+spell+suffix+'_C':
                        errors.append('真实成本/冷却类引用错误: '+path+' '+field)
        # 原普攻和被动仍保留稳定开发身份，不能因为三个魔法替换而损坏其他槽。
        for index,slot in [(0,'primary' if hero=='Rat' else 'basicattack'),(1,'passive')]:
            expected_id='dba.ability.'+hero.lower()+'_dev_'+slot+'@1'
            if identifier(grants[index].get_editor_property('AbilityId'))!=expected_id or str(ui[index].get_editor_property('AbilityId'))!=expected_id:
                errors.append('原普攻/被动被意外替换: '+hero)
    for spell in EXPECTED:
        icon = "/DBAUIPack_Core/UI/Abilities/MagicTests/T_DBA_Dev" + spell
        texture = unreal.load_asset(icon)
        if not isinstance(texture, unreal.Texture2D):
            errors.append("图标未导入: " + icon)
        for stage in ("Flight", "Impact"):
            path = "/DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NS_DBA_Dev" + spell + stage
            # 冰箭已由 RefineIceArrowConceptMonolith 发布专用系统；稳定定义仍保留原逻辑ID。
            # 独立夹具核对真实软引用与已保存的700厘米边界，不能再要求旧占位系统。
            if spell == "Frostbolt":
                path = "/DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_" + stage
            inspected.append(path)
            system=unreal.load_asset(path)
            if not isinstance(system, unreal.NiagaraSystem):
                errors.append("Niagara资产未制作: " + path)
            else:
                # 独立已批准边界：高速半秒尾迹不能退回仅包住核心的小盒；命中仍保持有限范围。
                bounds=system.get_editor_property('FixedBounds')
                extent=700.0 if spell=='Frostbolt' else (950.0 if stage=='Flight' else 250.0)
                if not bounds.is_valid or any(abs(getattr(bounds.min,axis)+extent)>0.01 or abs(getattr(bounds.max,axis)-extent)>0.01 for axis in ('x','y','z')):
                    errors.append('真实保存GPU包围盒不符: '+path)
            definition=unreal.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_Dev'+spell+stage)
            if not definition or identifier(definition.get_editor_property('LogicalId'))!='presentation.dba.development.magic.'+spell.lower()+'.'+stage.lower()+'@1':
                errors.append('VFX定义逻辑ID不符: '+spell+stage)
            elif definition.get_editor_property('NiagaraSystem')!=unreal.load_asset(path): errors.append('VFX软系统引用缺失或指向其他资源: '+path)
    report = {"Project": str(project), "Inspected": inspected, "Errors": errors,
              "Passed": not errors, "NetworkVerified": False, "ManualVisualAccepted": False}
    output = project.parents[1] / "Saved/Validation/MagicHUD20261012/MagicAssets.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    if errors:
        raise RuntimeError("开发魔法资产未通过: " + json.dumps(errors, ensure_ascii=False))
    print(json.dumps({"Passed": True, "Report": str(output)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
