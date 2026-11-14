"""通过直接Monolith导入每插件独立封面，保留源图RGB；不注册新业务目录。"""
import json
import os
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[3]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(ROOT/'Game/DivineBeastsArena.uproject').resolve()
BASE='/DBAUIPack_Core/Development/PluginDemos'
cards=json.loads(Path(__file__).with_name('PluginHubCards.json').read_text(encoding='utf-8'))['Cards']
manifest=json.loads((ROOT/'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
assert {r['PluginId'] for r in cards}=={r['plugin_id'] for r in manifest['plugins']}
sources={r['PluginId']:Path(__file__).with_name('Art')/(r['PluginId'].replace('_','')+'.png') for r in cards}
missing=[str(p) for p in sources.values() if not p.is_file()]
if missing: raise RuntimeError('Missing independent source images: '+str(missing))
tasks=[]
for identity,source in sources.items():
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(source))
    task.set_editor_property('destination_path',BASE+'/Textures/Plugins')
    task.set_editor_property('destination_name','TPluginHub'+identity.replace('_',''))
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',False)
    task.set_editor_property('save',False)
    if not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Textures/Plugins/TPluginHub'+identity.replace('_','')):
        tasks.append(task)
if tasks: unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
images={}
records=[]
for identity,source in sources.items():
    image=unreal.load_asset(BASE+'/Textures/Plugins/TPluginHub'+identity.replace('_',''))
    assert isinstance(image,unreal.Texture2D),identity
    image.set_editor_property('srgb',True)
    image.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    image.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    image.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    image.set_editor_property('use_legacy_gamma',False)
    image.set_editor_property('max_texture_size',512)
    image.set_editor_property('never_stream',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(image,False)
    images[unreal.Name(identity)]=image
    records.append({'PluginId':identity,'Source':str(source.relative_to(ROOT)),'Texture':image.get_path_name(),'MaxTextureSize':512,'SRGB':True,'Compression':'TC_EDITOR_ICON'})
bp=unreal.load_asset(BASE+'/UI/WBP_DBA_PluginPortalCard')
cdo=unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('CardImages',images)
cdo.set_editor_property('CardTitles',{unreal.Name(r['PluginId']):unreal.Text(r['Title']) for r in cards})
cdo.set_editor_property('CardDescriptions',{unreal.Name(r['PluginId']):unreal.Text(r['Description']) for r in cards})
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
result={'PID':os.getpid(),'Count':len(records),'Images':records,'Status':'NativeImportedAndSaved','Reload':'Pending','HumanReview':'Pending'}
(ROOT/'Saved/Validation/PluginHubCards20261114/IndependentImagesNative.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print('Native independent textures saved:',len(records),'card map:',len(cdo.get_editor_property('CardImages')))
