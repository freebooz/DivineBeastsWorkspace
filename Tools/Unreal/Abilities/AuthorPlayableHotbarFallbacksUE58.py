"""通过Monolith显式执行：补齐十二生肖6/7/8槽的真实可替换GAS映射。

保留既有1—5号技能、被动、Hero/AbilitySet身份和Profile全部作者条目。
新技能采用原角色已登记技能ID与V5批准名称，复用真实占位GA/GE，不批准V6最终机制。
"""
import copy
import importlib.util
import json
import shutil
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT/'Saved/Validation/SkillBar20261021'
ASSET_ROOT = '/DBAGameplay/Abilities/Placeholders/HotbarFallbacks'
ORDER = ('Primary','Slot1','Slot2','Slot4','Slot3','Slot5','Slot6','Slot7')
# 槽位6/7/8分别复用恢复、冰箭和三连奥术；共享原技能冷却组，提示明确告知。
MODES = {'Slot5':('Primary','治疗恢复'), 'Slot6':('Frostbolt','寒冰箭'), 'Slot7':('ArcaneMissiles','奥术飞弹')}

def require(value, message):
    if not value: raise RuntimeError(message)

def author():
    require(Path(unreal.Paths.get_project_file_path()).resolve() == ROOT/'Game/DivineBeastsArena.uproject', '非正式工程')
    spec=importlib.util.spec_from_file_location('hotbar_asset_helpers',str(ROOT/'Tools/Unreal/Abilities/AuthorZodiacPlaceholderAbilities.py'))
    helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
    design=json.loads((ROOT/'Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json').read_text(encoding='utf-8'))
    names={e[0]:e[1] for e in json.loads((ROOT/'Docs/Design/ApprovedSkillDisplayNames_V5.json').read_text(encoding='utf-8'))['entries']}
    source_table=unreal.load_asset(helper.ROOT+'/DT_DBA_PlaceholderBalance')
    source_rows={r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(source_table))}
    plans=[]
    for hero in design['heroes']:
        key=hero['heroId'].split('.')[-1]
        aset=unreal.load_asset(helper.ROOT+'/DA_DBA_'+key+'_PlaceholderAbilitySet')
        profile=unreal.load_asset('/DBAHeroPack_'+key+'/UI/Abilities/DA_DBA_'+key+'_PlaceholderUIProfile')
        require(aset and profile,'缺少既有角色资产 '+key)
        entries=list(profile.get_editor_property('Entries'))
        for skill in hero['skills']:
            slot=skill['slotId']
            if slot not in MODES: continue
            mode,label=MODES[slot]; identity=skill['abilityId']
            source_bp=unreal.load_asset(helper.ROOT+'/GA_DBA_'+key+'_Placeholder'+mode)
            source_def=unreal.load_asset(helper.ROOT+'/DA_DBA_'+key+'_Placeholder'+mode)
            require(source_bp and source_def and identity in names,'映射源不完整 '+identity)
            old_ui=next(e for e in entries if str(e.get_editor_property('AbilityId'))==('dba.ability.'+key.lower()+'_placeholder_'+mode.lower()+'@1'))
            ui=next((e for e in entries if str(e.get_editor_property('AbilityId'))==identity),None)
            icon=ui.get_editor_property('Icon') if ui else old_ui.get_editor_property('Icon')
            require(icon,'无可复用的真实图标 '+identity)
            base='DBA_'+key+'_Hotbar'+slot
            plans.append({'hero':key,'identity':identity,'slot':slot,'mode':mode,'label':label,'name':names[identity],
                          'bp':source_bp,'definition':source_def,'set':aset,'profile':profile,'icon':icon,
                          'def_path':ASSET_ROOT+'/DA_'+base,'ga_path':ASSET_ROOT+'/GA_'+base,
                          'row':key+'_'+slot+'_L1'})
    require(len(plans)==36 and len({p['identity'] for p in plans})==36,'映射数量或身份重复')
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),'存在未保存作者资产，先保留现场')
    # 先保留所有将被修改的真实字节，禁止覆盖其它英雄作者条目。
    originals={p['set'].get_name()+'.uasset' for p in plans}|{p['profile'].get_name()+'.uasset' for p in plans}|{'WBP_DBA_UI_AbilityBar.uasset'}
    index={p.name:p for p in (ROOT/'Game/Plugins/DivineBeasts').rglob('*.uasset') if p.name in originals}
    require(set(index)==originals,'备份路径有遗漏')
    for name,path in index.items():
        dest=OUT/'Backup'/path.relative_to(ROOT)
        if not dest.exists(): dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
    rows=[]
    for p in plans:
        row=copy.deepcopy(source_rows[p['hero']+'_Placeholder_'+p['mode']+'_L1'])
        row['Name']=p['row'];row['AbilityId']=p['identity'];rows.append(row)
    factory=unreal.DataTableFactory()
    factory.set_editor_property('Struct',source_table.get_editor_property('RowStruct'))
    table=helper.make_asset(ASSET_ROOT+'/DT_DBA_PlayableHotbarFallbackBalance',unreal.DataTable,factory)
    require(unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False)),'数值表导入失败')
    helper.save(table)
    report=[]
    for p in plans:
        definition=unreal.load_asset(p['def_path']) if unreal.EditorAssetLibrary.does_asset_exist(p['def_path']) else unreal.EditorAssetLibrary.duplicate_asset(p['definition'].get_path_name(),p['def_path'])
        definition.set_editor_property('LogicalId',helper.logical(p['identity']))
        definition.set_editor_property('BalanceTable',table)
        definition.set_editor_property('LevelRowNames',[p['row']])
        require(not definition.get_editor_property('bEightSlotDraftOnly') and not definition.get_editor_property('bV6NativeApproved'),'不得修改最终V6签审状态')
        helper.save(definition)
        bp=unreal.load_asset(p['ga_path']) if unreal.EditorAssetLibrary.does_asset_exist(p['ga_path']) else unreal.EditorAssetLibrary.duplicate_asset(p['bp'].get_path_name(),p['ga_path'])
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('AbilityDefinitionId',helper.primary(p['identity']))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);helper.save(bp)
        cls=unreal.EditorAssetLibrary.load_blueprint_class(p['ga_path'])
        grants=list(p['set'].get_editor_property('Abilities'))
        grants=[g for g in grants if g.get_editor_property('InputTag').export_text()!=helper.tag('Platform.Ability.Input.DivineBeasts.'+p['slot']).export_text()]
        grant=unreal.GamePlatformAbilityGrant()
        require(grant.import_text('(AbilityId='+helper.logical(p['identity']).export_text()+',AbilityClass="'+cls.get_path_name()+'",AbilityLevel=1,InputTag='+helper.tag('Platform.Ability.Input.DivineBeasts.'+p['slot']).export_text()+')'),'授权结构错误')
        grants.append(grant);p['set'].set_editor_property('Abilities',grants)
        deps=list(p['set'].get_editor_property('RequiredDefinitions'))
        if all(str(d.get_editor_property('PrimaryAssetName'))!=p['identity'] for d in deps): deps.append(helper.primary(p['identity']))
        p['set'].set_editor_property('RequiredDefinitions',deps);helper.save(p['set'])
        entries=list(p['profile'].get_editor_property('Entries'))
        ui=next((e for e in entries if str(e.get_editor_property('AbilityId'))==p['identity']),None)
        if ui is None:
            ui=unreal.DivineBeastsAbilityUIEntry();ui.set_editor_property('AbilityId',unreal.Name(p['identity']));entries.append(ui)
        # 明确说明临时行为及共享CD；不把恢复写成已实现护盾、冰箭写成已实现控制。
        ui.set_editor_property('DisplayName',unreal.Text(p['name']))
        ui.set_editor_property('Description',unreal.Text('临时映射：'+p['label']+'。当前复用已实现技能及其服务器数值，与同类占位技能共享冷却；本角色最终专用机制尚未实现，后续保持技能编号替换。'))
        ui.set_editor_property('Icon',p['icon'])
        # UE结构体数组按值；用赋回确保现有条目的修改也持久化。
        entries=[ui if str(e.get_editor_property('AbilityId'))==p['identity'] else e for e in entries]
        p['profile'].set_editor_property('Entries',entries);helper.save(p['profile'])
        report.append({k:p[k] for k in ('hero','identity','slot','mode','label','name','def_path','ga_path')})
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'FallbackMappings.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('SSLM_HOTBAR_MAPPINGS_SAVED='+str(len(report)))
    return report

if __name__=='__main__': author()
