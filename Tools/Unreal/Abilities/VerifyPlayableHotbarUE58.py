"""只读检查正式技能栏与十二生肖授权集；须由Monolith在真实UE5.8执行。"""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
HEROES = ('Rat','Ox','Tiger','Rabbit','Dragon','Snake','Horse','Goat','Monkey','Rooster','Dog','Boar')
EXPECTED = {'Primary','Slot1','Slot2','Slot3','Slot4','Slot5','Slot6','Slot7'}

def verify(label='check'):
    errors, heroes = [], []
    bp = unreal.load_asset('/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilityBar')
    if unreal.get_default_object(bp.generated_class()).get_editor_property('bPreviewSelectedHeroIcons'):
        errors.append('正式技能栏仍为禁止施法的美术预览模式')
    for hero in HEROES:
        aset = unreal.load_asset('/DBAGameplay/Abilities/Placeholders/DA_DBA_'+hero+'_PlaceholderAbilitySet')
        profile = unreal.load_asset('/DBAHeroPack_'+hero+'/UI/Abilities/DA_DBA_'+hero+'_PlaceholderUIProfile')
        ui = {str(e.get_editor_property('AbilityId')):e for e in profile.get_editor_property('Entries')}
        seen, classes = set(), set()
        for g in aset.get_editor_property('Abilities'):
            exported = g.get_editor_property('InputTag').export_text()
            if 'Platform.Ability.Input.DivineBeasts.' not in exported:
                continue
            slot = exported.split('DivineBeasts.')[1].split('"')[0]
            if slot in seen: errors.append(hero+':重复输入槽 '+slot)
            seen.add(slot)
            cls = g.get_editor_property('AbilityClass')
            cdo = unreal.get_default_object(cls)
            if cls.get_path_name() in classes: errors.append(hero+':重复技能类')
            classes.add(cls.get_path_name())
            identity = cdo.get_editor_property('AbilityDefinitionId').get_editor_property('PrimaryAssetName')
            entry = ui.get(str(identity))
            if not entry or not entry.get_editor_property('Icon'):
                errors.append(hero+':缺少真实图标 '+str(identity))
        if seen != EXPECTED: errors.append(hero+':缺少可施放槽 '+str(sorted(EXPECTED-seen)))
        heroes.append({'hero':hero,'activeSlots':sorted(seen),'grantCount':len(aset.get_editor_property('Abilities'))})
    report={'passed':not errors,'errors':errors,'heroes':heroes,'networkAccepted':False,'humanAccepted':False}
    folder=ROOT/'Saved/Validation/SkillBar20261021';folder.mkdir(parents=True,exist_ok=True)
    (folder/(label+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))
    return report

if __name__ == '__main__': verify()
