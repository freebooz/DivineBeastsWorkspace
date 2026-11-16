"""通过Monolith在真实UE中建立第三层植物原型库，不改现有地图/世界资源。"""
import unreal as u,json
from pathlib import Path
ROOT='/DBAFoliagePack_Core/Foliage'
SOURCE='/DBAWorldPack_Village/PCG/HillsideForest'
TOOLS=u.AssetToolsHelpers.get_asset_tools()

def duplicate(source,target):
    asset=u.load_asset(target) if u.EditorAssetLibrary.does_asset_exist(target) else u.EditorAssetLibrary.duplicate_asset(source,target)
    assert asset,target
    return asset

def author():
    for name in ['Trees','Shrubs','Grass','Flowers','Materials','FoliageTypes','PCG']:
        folder=ROOT+'/'+name
        assert u.EditorAssetLibrary.does_directory_exist(folder) or u.EditorAssetLibrary.make_directory(folder)
    master=duplicate(SOURCE+'/Materials/M_DBA_Review_TreeSimple',ROOT+'/Materials/M_DBA_Foliage_PrototypeTree')
    mapping={}
    for name in ['Bark','Blossom','Canopy','LeafHighlight']:
        original=SOURCE+'/Materials/MI_DBA_Review_'+name
        material=duplicate(original,ROOT+'/Materials/MI_DBA_Foliage_Prototype_'+name)
        u.MaterialEditingLibrary.set_material_instance_parent(material,master)
        mapping[u.load_asset(original).get_path_name()]=material
        mapping[material.get_path_name()]=material
        assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    mesh=duplicate(SOURCE+'/SM_DBA_Review_WideCrown',ROOT+'/Trees/SM_DBA_Foliage_PrototypeTreeWideCrown')
    for index,slot in enumerate(mesh.get_editor_property('static_materials')):
        original=slot.get_editor_property('material_interface')
        assert original and original.get_path_name() in mapping,('Unknown source material',str(original))
        mesh.set_material(index,mapping[original.get_path_name()])
    assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
    path=ROOT+'/FoliageTypes/FT_DBA_Tree_PrototypeWideCrown'
    ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else TOOLS.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
    assert ft
    ft.set_editor_property('mesh',mesh)
    ft.set_editor_property('density',.03)
    ft.set_editor_property('radius',400.)
    ft.set_editor_property('align_to_normal',False)
    ft.set_editor_property('collision_with_world',False)
    body=ft.get_editor_property('body_instance')
    body.set_editor_property('collision_enabled',u.CollisionEnabled.NO_COLLISION)
    ft.set_editor_property('body_instance',body)
    for name in ['scale_x','scale_y','scale_z']:
        interval=ft.get_editor_property(name);interval.set_editor_property('min',.85);interval.set_editor_property('max',1.15);ft.set_editor_property(name,interval)
    interval=ft.get_editor_property('cull_distance');interval.set_editor_property('min',5000);interval.set_editor_property('max',15000);ft.set_editor_property('cull_distance',interval)
    assert u.EditorAssetLibrary.save_loaded_asset(ft,False)
    path=ROOT+'/PCG/DA_DBA_Foliage_PrototypeTreeMeshSet'
    if u.EditorAssetLibrary.does_asset_exist(path):definition=u.load_asset(path)
    else:
        factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',u.GamePlatformPCGMeshSetDefinition)
        definition=TOOLS.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],u.GamePlatformPCGMeshSetDefinition,factory)
    assert definition
    definition.set_editor_property('logical_id',u.GamePlatformId(namespace='divinebeasts.foliage',name='prototype_trees',logical_version=1))
    entry=u.GamePlatformPCGMeshSetEntry(mesh=mesh,weight=1.,static_collision=False,
        cast_shadow=True,cull_distance_cm=15000.,nanite_preferred=False)
    definition.set_editor_property('entries',[entry])
    assert u.EditorAssetLibrary.save_loaded_asset(definition,False)
    u.MaterialEditingLibrary.recompile_material(master)
    assert u.EditorAssetLibrary.save_loaded_asset(master,False)
    # 只审计本作者拥有的8项资产，不改写并行导入的柳树等资源。
    assets=sorted({a.get_path_name() for a in [master,mesh,ft,definition]+list(mapping.values())})
    deps={}
    options=u.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
    registry=u.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([ROOT],True)
    for asset in assets:
        package=asset.split('.')[0]
        deps[package]=[str(x) for x in (registry.get_dependencies(package,options) or [])]
    wrong=[p for paths in deps.values() for p in paths if p.startswith('/DBA') and not p.startswith('/DBAFoliagePack_Core/')]
    assert not wrong,wrong
    out=Path(u.Paths.project_dir()).resolve().parent/'Saved/Validation/FoliagePack20261024';out.mkdir(parents=True,exist_ok=True)
    report={'assets':assets,'nativeAssetCount':len(assets),'dependencies':deps,'reverseWorldDependencies':wrong,'sourceWorldUntouched':True,'resourceMaturity':'Prototype','runtimePromoted':False}
    (out/'NativeAuthoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['dependencies','assets']}))

author()
