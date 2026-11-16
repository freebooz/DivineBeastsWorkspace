"""Monolith原生SpeedTree导入；来源为用户下载目录的两个指定ST9及同名前缀纹理。"""
import unreal as u,json
from pathlib import Path
BASE=Path(u.Paths.project_dir()).resolve().parent
PACK=BASE/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFoliagePack_Core'
ROOT='/DBAFoliagePack_Core/Foliage/Grass'
results=[]
for species in ['BostonFern','WesternSwordFern']:
    name=species+'_Low';source=PACK/'SourceArt/Grass'/name/(name+'.st9')
    destination=ROOT+'/'+species
    mesh_name='SM_DBA_Foliage_'+name
    mesh_path=destination+'/'+mesh_name
    if not u.EditorAssetLibrary.does_asset_exist(mesh_path):
        options=u.SpeedTreeImportData()
        options.set_editor_property('tree_scale',1.)
        options.set_editor_property('include_collision',False)
        options.set_editor_property('make_materials_check',True)
        options.set_editor_property('include_normal_map_check',True)
        options.set_editor_property('include_vertex_processing_check',True)
        options.set_editor_property('include_wind_check',True)
        task=u.AssetImportTask();task.set_editor_property('filename',str(source))
        task.set_editor_property('destination_path',destination);task.set_editor_property('destination_name',mesh_name)
        task.set_editor_property('automated',True);task.set_editor_property('save',True)
        task.set_editor_property('replace_existing',False);task.set_editor_property('factory',u.SpeedTreeImportFactory())
        task.set_editor_property('options',options)
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        imported=[str(p) for p in task.get_editor_property('imported_object_paths')]
        meshes=[u.load_asset(p) for p in imported if isinstance(u.load_asset(p),u.StaticMesh)]
        assert len(meshes)==1,(species,imported)
        mesh=meshes[0]
        if mesh.get_path_name().split('.')[0]!=mesh_path:
            assert u.EditorAssetLibrary.rename_asset(mesh.get_path_name(),mesh_path)
    mesh=u.load_asset(mesh_path);assert isinstance(mesh,u.StaticMesh),mesh_path
    assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
    bounds=mesh.get_bounds()
    results.append({'species':species,'mesh':mesh.get_path_name(),'lodCount':mesh.get_num_lods(),
        'boundsOrigin':str(bounds.origin),'boundsExtent':str(bounds.box_extent),
        'materials':[s.get_editor_property('material_interface').get_path_name() for s in mesh.get_editor_property('static_materials')],
        'source':str(source)})
# 将原生导入器的默认名字规范化；只改本轮尚未发布资产，UE自动维护引用。
renames={
 'BostonFern':{'BostonFern_Low_Mat':'MI_DBA_BostonFern_Leaves',
   'BostonFern_Low_Color_png_Tex':'T_DBA_BostonFern_Color','BostonFern_Low_Normal_png_Tex':'T_DBA_BostonFern_Normal'},
 'WesternSwordFern':{'WesternSwordFern_Low_Mat':'MI_DBA_WesternSwordFern_Leaves',
   'SwordFernFiddlehead_Mat':'MI_DBA_WesternSwordFern_Fiddlehead',
   'WesternSwordFern_Low_Color_png_Tex':'T_DBA_WesternSwordFern_Color',
   'WesternSwordFern_Low_Normal_png_Tex':'T_DBA_WesternSwordFern_Normal',
   'WesternSwordFern_Low_Subsurface_png_Tex':'T_DBA_WesternSwordFern_Subsurface'}}
for species,mapping in renames.items():
    for old,new in mapping.items():
        source=ROOT+'/'+species+'/'+old;target=ROOT+'/'+species+'/'+new
        if not u.EditorAssetLibrary.does_asset_exist(target):
            assert u.EditorAssetLibrary.rename_asset(source,target),(source,target)
# SpeedTree的自动导入只保证主网格保存；明确保存本任务生成的材质和纹理依赖。
for path in u.EditorAssetLibrary.list_assets(ROOT,True,False):
    assert u.EditorAssetLibrary.save_loaded_asset(u.load_asset(path),False)
for row in results:
    mesh=u.load_asset(row['mesh'])
    row['materials']=[s.material_interface.get_path_name() for s in mesh.get_editor_property('static_materials')]
out=BASE/'Saved/Validation/VillageFerns20261024/NativeImport.json'
out.write_text(json.dumps({'nativeImported':True,'models':results,'mapModified':False},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(results))
