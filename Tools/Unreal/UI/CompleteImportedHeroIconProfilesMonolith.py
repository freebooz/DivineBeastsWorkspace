"""SSLM英雄图标导入后的唯一UIProfile接线与只读验证，必须由Monolith执行。

所属第三层编辑器工具；调用方提供已核验的IntegrationPlan.json。默认inspect，
apply仅增补现有唯一Profile的展示元数据，不授予GAS、不创建第二个同英雄主资产。
verify核对原有条目保留、图标来源及无授权VM；失败抛出异常并保留备份/证据。
不连接后端、不保存地图、不启动客户端，订阅与生命周期复用现有ViewModel。
"""
from pathlib import Path
import hashlib
import json
import shutil
import unreal

ROOT=Path(__file__).resolve().parents[3]
PLAN=Path(globals().get('PROFILE_PLAN',ROOT/'Saved/Validation/HeroSkillIconWorkflow20261019/IntegrationPlan.json'))
MODE=globals().get('PIPELINE_MODE','inspect')
OUT=PLAN.parent

def require(condition,message):
    """条件失败立即停止，不能把工具返回当作资产验收。"""
    if not condition:
        raise RuntimeError(message)

def icon_path(entry):
    """从真实条目读取软资源路径，统一为包名，不按中文名称猜测对应关系。"""
    icon=entry.get_editor_property('icon')
    return icon.get_path_name().split('.')[0] if icon else ''

def snapshot_entry(entry):
    """旧表现条目的不可变比较快照；不携带可修改玩法字段。"""
    return {'id':str(entry.get_editor_property('ability_id')),
        'name':str(entry.get_editor_property('display_name')),
        'description':str(entry.get_editor_property('description')),'icon':icon_path(entry)}

def protect():
    """保护旧纹理和权威数据，只允许清单内Profile发生本次明确授权的增补。"""
    baseline=json.loads((OUT/'ProtectedFiles.json').read_text(encoding='utf-8'))
    require(all(hashlib.sha256((ROOT/f).read_bytes()).hexdigest()==h for f,h in baseline.items()),'保护文件被修改')

def inspect_hero(hero,strict):
    """完整读取真实Profile和32项以内目标映射，冲突条目不覆盖。"""
    lib=unreal.EditorAssetLibrary
    profile=lib.load_asset(hero['profilePath'])
    require(isinstance(profile,unreal.DivineBeastsAbilityUIProfile),'现有唯一Profile缺失')
    require(str(profile.get_editor_property('hero_definition_id'))==hero['heroId'],'Profile英雄身份错误')
    assets=unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/DBAHeroPack_'+hero['hero']+'/UI/Abilities',recursive=True)
    matches=[]
    for data in assets:
        if str(data.asset_class_path.asset_name)=='DivineBeastsAbilityUIProfile':
            candidate=lib.load_asset(str(data.package_name))
            if str(candidate.get_editor_property('hero_definition_id'))==hero['heroId']:
                matches.append(str(data.package_name))
    require(matches==[hero['profilePath']],'同英雄Profile主资产不唯一')
    entries=list(profile.get_editor_property('entries'))
    current=[snapshot_entry(e) for e in entries]
    ids=[e['id'] for e in current]
    require(len(ids)==len(set(ids)) and len(ids)<=64,'Profile存在重复ID或超过上限')
    old_path=OUT/('OriginalEntries_'+hero['hero']+'.json')
    if not old_path.exists():
        require(MODE=='inspect','缺少明确接线前快照')
        old_path.write_text(json.dumps(current,ensure_ascii=False,indent=2),encoding='utf-8')
    original=json.loads(old_path.read_text(encoding='utf-8'))
    require(all(item in current for item in original),'接线删除或覆盖了原有条目')
    by_id={e['id']:e for e in current}
    missing=[]
    for row in hero['entries']:
        texture=lib.load_asset(row['texturePath'])
        require(isinstance(texture,unreal.Texture2D),'真实导入Texture2D缺失')
        require(texture.get_editor_property('srgb') and texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_EDITOR_ICON,'纹理颜色/压缩合同错误')
        require(str(lib.find_asset_data(row['texturePath']).get_tag_value('Dimensions'))=='48x48','原生运行图不是48像素')
        if row['abilityId'] in by_id:
            entry=by_id[row['abilityId']]
            require(entry['name']==row['displayNameZh'] and entry['icon']==row['texturePath'] and entry['description'].strip(),'已有映射冲突，拒绝覆盖')
        else:
            missing.append(row)
    if strict:
        require(not missing,hero['hero']+' 尚缺 '+str(len(missing))+' 项UIProfile映射')
    # 真正的既有ViewModel必须在没有权威Loadout时保持空，不从美术清单制造可施放槽位。
    vm=unreal.new_object(unreal.DivineBeastsAbilityBarViewModel)
    require(len(vm.get_slots())==0 and len(vm.get_slot_details())==0,'未授权VM出现伪造槽位')
    vm.unbind_from_loadout()
    return profile,entries,missing,{'hero':hero['hero'],'entries':len(entries),'mapped':len(hero['entries'])-len(missing),'missing':len(missing),'uniqueProfile':True,'unboundViewModelEmpty':True}

def main():
    """先全量预检，再串行备份/增补/保存；重跑已正确映射时不重写原资产。"""
    require(Path(unreal.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject','工程身份不符')
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.0'),'引擎版本不符')
    require(MODE in ('inspect','apply','verify','reload'),'未知执行模式')
    plan=json.loads(PLAN.read_text(encoding='utf-8'))
    protect()
    checks=[(hero,*inspect_hero(hero,MODE in ('verify','reload'))) for hero in plan['heroes']]
    if MODE=='apply':
        for hero,profile,entries,missing,summary in checks:
            if not missing:
                continue
            disk=ROOT/hero['profileFile']
            backup=OUT/'Backups'/hero['hero']/disk.name
            require(not backup.exists(),'已有备份，不能覆盖先前恢复点')
            backup.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(disk,backup)
            require(len(entries)+len(missing)<=64,'新增条目超过Profile上限')
            for row in missing:
                entry=unreal.DivineBeastsAbilityUIEntry()
                entry.set_editor_property('ability_id',unreal.Name(row['abilityId']))
                entry.set_editor_property('display_name',unreal.Text(row['displayNameZh']))
                entry.set_editor_property('description',unreal.Text(row['descriptionZh']))
                entry.set_editor_property('icon',unreal.load_asset(row['texturePath']))
                entries.append(entry)
            profile.set_editor_property('entries',entries)
            require(unreal.EditorAssetLibrary.save_loaded_asset(profile,only_if_is_dirty=False),'Profile保存失败')
    elif MODE=='reload':
        packages=[profile.get_outermost() for hero,profile,entries,missing,summary in checks]
        ok,error=unreal.EditorLoadingAndSavingUtils.reload_packages(packages,unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
        require(ok,'Profile磁盘包重载失败：'+str(error))
    if MODE!='inspect':
        checks=[(hero,*inspect_hero(hero,True)) for hero in plan['heroes']]
    protect()
    summaries=[check[-1] for check in checks]
    report={'mode':MODE,'heroes':summaries,'mapped':sum(x['mapped'] for x in summaries),
        'missing':sum(x['missing'] for x in summaries),'gameplayChanged':False,'finalHUDVerified':False}
    (OUT/('Profiles_'+MODE+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))

main()
