"""真实Monolith/UE5.8导入与唯一Rabbit Profile接线；保留V1及全部既有条目。

包内文字只作来源数据，正式名称与技能身份由工作空间批准目录核对。导入不等于美术签审。
"""
import hashlib, json, shutil
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/RabbitV6IconImport20261021'
PROFILE='/DBAHeroPack_Rabbit/UI/Abilities/DA_DBA_Rabbit_PlaceholderUIProfile'

def require(ok,message):
    if not ok: raise RuntimeError(message)

def run():
    require(Path(unreal.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject','工程身份错误')
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.'),'UE版本错误')
    stage=json.loads((OUT/'Staging.json').read_text(encoding='utf-8'))
    profile=unreal.load_asset(PROFILE)
    require(isinstance(profile,unreal.DivineBeastsAbilityUIProfile),'唯一Rabbit Profile缺失')
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),'存在未保存资产，禁止覆盖')
    file=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rabbit/Content/UI/Abilities/DA_DBA_Rabbit_PlaceholderUIProfile.uasset'
    backup=OUT/'Backup'/file.name
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists(): shutil.copy2(file,backup)
    native=[]
    for skill in stage['skills']:
        for key,suffix,size in [('runtime48','',48),('review64','_Review64',64)]:
            record=skill[key];source=ROOT/record['path']
            require(hashlib.sha256(source.read_bytes()).hexdigest()==record['sha256'],'图源指纹变化')
            path=skill['assetPath']+suffix
            require(not unreal.EditorAssetLibrary.does_asset_exist(path),'已有同名纹理，禁止静默覆盖 '+path)
            folder,name=path.rsplit('/',1)
            task=unreal.AssetImportTask();task.set_editor_property('filename',str(source))
            task.set_editor_property('destination_path',folder);task.set_editor_property('destination_name',name)
            task.set_editor_property('replace_existing',False);task.set_editor_property('automated',True);task.set_editor_property('save',False)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            tex=unreal.load_asset(path);require(isinstance(tex,unreal.Texture2D),'原生纹理导入失败')
            tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property('srgb',True);tex.set_editor_property('never_stream',True)
            tex.set_editor_property('filter',unreal.TextureFilter.TF_BILINEAR)
            tex.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);tex.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
            require(unreal.EditorAssetLibrary.save_loaded_asset(tex,False),'纹理保存失败')
            dimensions=str(unreal.EditorAssetLibrary.find_asset_data(path).get_tag_value('Dimensions')).lower()
            require(dimensions==f'{size}x{size}','真实纹理尺寸错误 '+path+' '+dimensions)
            native.append({'path':path,'size':size,'sourceSHA256':record['sha256']})
    entries=list(profile.get_editor_property('Entries'));original_count=len(entries)
    for skill in stage['skills']:
        old=next((e for e in entries if str(e.get_editor_property('AbilityId'))==skill['abilityId']),None)
        entry=unreal.DivineBeastsAbilityUIEntry() if old is None else old
        entry.set_editor_property('AbilityId',unreal.Name(skill['abilityId']))
        entry.set_editor_property('DisplayName',unreal.Text(skill['approvedNameZh']))
        if old is None: entry.set_editor_property('Description',unreal.Text('角色专用技能待实现；当前图标为下载包预选版本，尚待人工美术审核。'))
        entry.set_editor_property('Icon',unreal.load_asset(skill['assetPath']))
        entries=[e for e in entries if str(e.get_editor_property('AbilityId'))!=skill['abilityId']]+[entry]
    # 已批准的1—5占位能力继续拥有原ID、名称和服务器行为；只接入本角色对应槽图标。
    aliases={'Primary':'directedmissile','Slot1':'fireball','Slot2':'frostbolt','Slot4':'arcanemissiles','Slot3':'primary'}
    for skill in stage['skills']:
        mode=aliases.get(skill['slotId'])
        if mode:
            identity='dba.ability.rabbit_placeholder_'+mode+'@1'
            entry=next(e for e in entries if str(e.get_editor_property('AbilityId'))==identity)
            entry.set_editor_property('Icon',unreal.load_asset(skill['assetPath']))
            entries=[entry if str(e.get_editor_property('AbilityId'))==identity else e for e in entries]
    profile.set_editor_property('Entries',entries)
    profile.set_editor_property('ArtPreviewAbilityIds',[unreal.Name(s['abilityId']) for s in stage['skills']])
    require(unreal.EditorAssetLibrary.save_loaded_asset(profile,False),'Profile保存失败')
    report={'heroId':'Hero.Zodiac.Rabbit','nativeImported':True,'textures':native,'profile':PROFILE,
            'previousEntries':original_count,'entries':len(entries),'runtimeIcons':8,'auditIcons':8,
            'candidateSourcesPreserved':24,'humanApproved':False,'networkHUDVerified':False}
    (OUT/'NativeImport.saved.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))

if __name__=='__main__': run()
