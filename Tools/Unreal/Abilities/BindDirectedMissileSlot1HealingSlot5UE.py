# -*- coding: utf-8 -*-
"""经Monolith执行的有界迁移：新增定向飞弹，保留原技能，仅移动治疗输入。"""
import importlib.util,json,shutil
from pathlib import Path
from datetime import datetime,timezone
import unreal
ROOT=Path(__file__).resolve().parents[3]
s=importlib.util.spec_from_file_location('placeholder_author',Path(__file__).with_name('AuthorZodiacPlaceholderAbilities.py'))
a=importlib.util.module_from_spec(s);s.loader.exec_module(a)
OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileSlotBinding'
OUT.mkdir(parents=True,exist_ok=True)

def duplicate(source,destination):
    if unreal.EditorAssetLibrary.does_asset_exist(destination):return unreal.load_asset(destination)
    result=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(destination.rsplit('/',1)[1],destination.rsplit('/',1)[0],unreal.load_asset(source))
    assert result,destination
    return result

def run():
    assert '-DirectedMissileV1OwnedReview' in unreal.SystemLibrary.get_command_line()
    assert Path(unreal.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
    assert hasattr(unreal.get_default_object(unreal.DivineBeastsPlaceholderGameplayAbility),'presentation_style_override')
    a.tag('DivineBeasts.Placeholder.Cooldown.DirectedMissile')
    table=unreal.load_asset(a.ROOT+'/DT_DBA_PlaceholderBalance')
    rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    snapshot=[];work=[]
    for hero in a.HEROES:
        aset=unreal.load_asset(a.ROOT+'/DA_DBA_'+hero+'_PlaceholderAbilitySet')
        profile=unreal.load_asset('/DBAHeroPack_'+hero+'/UI/Abilities/DA_DBA_'+hero+'_PlaceholderUIProfile')
        grants=list(aset.get_editor_property('Abilities'));entries=list(profile.get_editor_property('Entries'))
        heal=[g for g in grants if str(g.get_editor_property('AbilityClass')).find('_PlaceholderPrimary.')>=0]
        assert len(heal)==1,(hero,'healing identity')
        assert str(heal[0].get_editor_property('InputTag').get_editor_property('TagName')) in ('Platform.Ability.Input.DivineBeasts.Primary','Platform.Ability.Input.DivineBeasts.Slot3')
        assert not any(str(g.get_editor_property('InputTag').get_editor_property('TagName')).endswith('.Slot3') and g!=heal[0] for g in grants)
        assert len(grants) in (5,6) and len(entries) in (5,6)
        snapshot.append(dict(hero=hero,grants=[g.export_text() for g in grants],ui=[e.export_text() for e in entries]))
        work.append((hero,aset,profile,grants,entries))
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    (OUT/(stamp+'_Before.json')).write_text(json.dumps(dict(rows=rows,heroes=snapshot),ensure_ascii=False,indent=2),encoding='utf8')
    # 先备份所有将修改的既有包；新资源不覆盖同名异构资产。
    backup=OUT/'Backups'/stamp
    for package in [table]+[o for _,aset,profile,_,_ in work for o in (aset,profile)]:
        path=package.get_path_name().split('.')[0]
        mount,tail=path[1:].split('/',1)
        paths=list((ROOT/'Game/Plugins').glob('**/'+mount+'/Content/'+tail+'.uasset'))
        assert len(paths)==1,(path,paths)
        dest=backup/paths[0].relative_to(ROOT);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(paths[0],dest)
    for hero,aset,profile,grants,entries in work:
        identity='dba.ability.'+hero.lower()+'_placeholder_directedmissile@1'
        prefix=a.ROOT+'/_DBA_'+hero+'_Placeholder'
        def path(kind,mode,suffix=''):return a.ROOT+'/'+kind+'_DBA_'+hero+'_Placeholder'+mode+suffix
        definition=duplicate(path('DA','Fireball'),path('DA','DirectedMissile'))
        definition.set_editor_property('LogicalId',a.logical(identity))
        definition.set_editor_property('LevelRowNames',[hero+'_Placeholder_DirectedMissile_L1'])
        row=next(r for r in rows if r['Name']==hero+'_Placeholder_Fireball_L1').copy()
        row.update(Name=hero+'_Placeholder_DirectedMissile_L1',AbilityId=identity)
        rows=[r for r in rows if r['Name']!=row['Name']]+[row]
        cool=duplicate(path('GE','Fireball','Cooldown'),path('GE','DirectedMissile','Cooldown'))
        unreal.BlueprintEditorLibrary.compile_blueprint(cool)
        cooldown_cls=unreal.EditorAssetLibrary.load_blueprint_class(path('GE','DirectedMissile','Cooldown'))
        cdo=unreal.get_default_object(cooldown_cls)
        comps=cdo.get_editor_property('GEComponents')
        for c in comps:
            if c.get_class().get_name()=='TargetTagsGameplayEffectComponent':
                inherited=c.get_editor_property('InheritableGrantedTagsContainer')
                assert inherited.import_text('(Added='+a.tags(['DivineBeasts.Placeholder.Cooldown.DirectedMissile']).export_text()+')')
                c.set_editor_property('InheritableGrantedTagsContainer',inherited)
        unreal.BlueprintEditorLibrary.compile_blueprint(cool);a.save(cool)
        cost=duplicate(path('GE','Fireball','Cost'),path('GE','DirectedMissile','Cost'));unreal.BlueprintEditorLibrary.compile_blueprint(cost);a.save(cost)
        bp=duplicate(path('GA','Fireball'),path('GA','DirectedMissile'));unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cls=unreal.EditorAssetLibrary.load_blueprint_class(path('GA','DirectedMissile'));cdo=unreal.get_default_object(cls)
        cdo.set_editor_property('AbilityDefinitionId',a.primary(identity))
        cdo.set_editor_property('PresentationStyleOverride','DBA.Development.DirectedMissile')
        cdo.set_editor_property('CooldownGameplayEffectClass',unreal.EditorAssetLibrary.load_blueprint_class(path('GE','DirectedMissile','Cooldown')))
        cdo.set_editor_property('PlaceholderCooldownTags',a.tags(['DivineBeasts.Placeholder.Cooldown.DirectedMissile']))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);a.save(bp);a.save(definition)
        # UE返回结构体拷贝：按索引写回数组，不能修改临时g后假称授权已更新。
        for i,g in enumerate(grants):
            if '_PlaceholderPrimary.' in str(g.get_editor_property('AbilityClass')):
                value=g.export_text().replace('Platform.Ability.Input.DivineBeasts.Primary','Platform.Ability.Input.DivineBeasts.Slot3')
                replacement=unreal.GamePlatformAbilityGrant();assert replacement.import_text(value);grants[i]=replacement
        grants=[g for g in grants if '_PlaceholderDirectedMissile.' not in str(g.get_editor_property('AbilityClass'))]
        grant=unreal.GamePlatformAbilityGrant()
        assert grant.import_text('(AbilityId='+a.logical(identity).export_text()+',AbilityClass="'+cls.get_path_name()+'",AbilityLevel=1,InputTag='+a.tag('Platform.Ability.Input.DivineBeasts.Primary').export_text()+')')
        grants.append(grant);aset.set_editor_property('Abilities',grants)
        deps=[d for d in aset.get_editor_property('RequiredDefinitions') if str(d.get_editor_property('PrimaryAssetName'))!=identity]
        aset.set_editor_property('RequiredDefinitions',deps+[a.primary(identity)])
        entries=[e for e in entries if str(e.get_editor_property('AbilityId'))!=identity]
        item=unreal.DivineBeastsAbilityUIEntry();item.set_editor_property('AbilityId',identity)
        item.set_editor_property('DisplayName',unreal.Text('定向飞弹'))
        item.set_editor_property('Description',unreal.Text('发射一枚定向能量飞弹，基础伤害40，射程20米，冷却2秒，不消耗气势。锁定施法时原目标并持续追踪；未选目标则直线飞行。'))
        item.set_editor_property('Icon',unreal.load_asset(a.MAGIC_ICONS['Fireball']))
        profile.set_editor_property('Entries',entries+[item]);a.save(profile)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False));a.save(table)
    # 新定义、GA、UI均保存成功后，最后发布授权集。
    for _,aset,_,_,_ in work:a.save(aset)
    report=dict(status='saved',heroes=12,slots={'1':'DirectedMissile','2':'Fireball','3':'Frostbolt','4':'ArcaneMissiles','5':'Primary (Healing)'},saved=a.SAVED,backup=str(backup))
    (OUT/'Binding.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('MISSILE_SLOT_BINDINGS_SAVED',len(a.SAVED))
if __name__=='__main__':run()
