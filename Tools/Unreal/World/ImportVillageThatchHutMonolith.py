# -*- coding: utf-8 -*-
"""通过Monolith在锁定UE编辑器中导入用户提供的ST9茅屋；不创建替代几何、不覆盖旧资产。"""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village'
DESTINATION = '/DBAWorldPack_Village/Environment/Buildings/ThatchHut'
MESH_NAME = 'SM_DBA_Village_ThatchHut_Low'
OUTPUT = ROOT / 'Saved/Validation/ThatchHut20261024'


def complete_materials():
    """接齐ST9引用的全部原图；遵循SpeedTree9的Normal+Roughness打包，不用BC5丢弃Alpha。"""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.EditorAssetLibrary
    for name in ['Walls', 'Roof', 'Sandstone', 'AcaciaBark', 'Grass', 'Ends', 'ThatchHut_Low']:
        material = lib.load_asset(DESTINATION + '/' + name + '_Mat')
        assert isinstance(material, unreal.MaterialInstanceConstant)
        for kind, parameter in [('Color', 'ColorOpacity'), ('Normal', 'NormalRoughness')]:
            texture_name = name + '_' + kind + '_png_Tex'
            texture = lib.load_asset(DESTINATION + '/' + texture_name) if lib.does_asset_exist(DESTINATION + '/' + texture_name) else None
            if texture is None:
                task = unreal.AssetImportTask()
                task.set_editor_property('filename', str(PACK / 'SourceArt/Buildings/ThatchHut_Low' / (name + '_' + kind + '.png')))
                task.set_editor_property('destination_path', DESTINATION)
                task.set_editor_property('destination_name', texture_name)
                task.set_editor_property('automated', True)
                task.set_editor_property('save', False)
                task.set_editor_property('replace_existing', False)
                tools.import_asset_tasks([task])
                texture = lib.load_asset(DESTINATION + '/' + texture_name)
            assert isinstance(texture, unreal.Texture2D)
            texture.set_editor_property('srgb', kind == 'Color')
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
            # 原图颜色与Alpha保持原始值，不沿用树叶导入器自动增加的最小Alpha。
            texture.set_editor_property('adjust_min_alpha', 0.0)
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, parameter, texture)
            target = DESTINATION + '/T_DBA_ThatchHut_' + name.replace('ThatchHut_Low', 'Atlas') + '_' + kind
            if texture.get_path_name().split('.')[0] != target:
                assert lib.rename_asset(texture.get_path_name(), target)
            assert lib.save_loaded_asset(texture, only_if_is_dirty=False)
        unreal.MaterialEditingLibrary.update_material_instance(material)
        target = DESTINATION + '/MI_DBA_ThatchHut_' + name.replace('ThatchHut_Low', 'Atlas')
        if material.get_path_name().split('.')[0] != target:
            assert lib.rename_asset(material.get_path_name(), target)
        assert lib.save_loaded_asset(material, only_if_is_dirty=False)
    mesh = lib.load_asset(DESTINATION + '/' + MESH_NAME)
    assert lib.save_loaded_asset(mesh, only_if_is_dirty=False)
    print('THATCH_HUT_MATERIALS_COMPLETE', 7, 14)


def import_model():
    """使用UE原生SpeedTree9导入器，保持原尺度与配套贴图，不在地图中自动散布建筑。"""
    assert Path(unreal.Paths.get_project_file_path()).resolve() == (ROOT / 'Game/DivineBeastsArena.uproject').resolve()
    mesh_path = DESTINATION + '/' + MESH_NAME
    assert not unreal.EditorAssetLibrary.does_asset_exist(mesh_path), '目标已存在，请先核对已有资产，禁止重复导入覆盖'
    options = unreal.SpeedTreeImportData()
    options.set_editor_property('tree_scale', 1.0)
    options.set_editor_property('import_geometry_type', unreal.ImportGeometryType.IGT_3D)
    options.set_editor_property('include_collision', True)
    options.set_editor_property('make_materials_check', True)
    options.set_editor_property('include_normal_map_check', True)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(PACK / 'SourceArt/Buildings/ThatchHut_Low/ThatchHut_Low.st9'))
    task.set_editor_property('destination_path', DESTINATION)
    task.set_editor_property('destination_name', MESH_NAME)
    task.set_editor_property('factory', unreal.SpeedTreeImportFactory())
    task.set_editor_property('options', options)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', False)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    assert isinstance(mesh, unreal.StaticMesh), '原生SpeedTree9模型导入失败'
    assets = list(unreal.EditorAssetLibrary.list_assets(DESTINATION, recursive=True, include_folder=False))
    bounds = mesh.get_bounds()
    report = {'mesh': mesh.get_path_name(), 'assets': assets, 'boundsOrigin': str(bounds.origin),
              'boundsExtent': str(bounds.box_extent), 'materials': [str(m.material_interface) for m in mesh.get_editor_property('static_materials')]}
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / 'Import.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('THATCH_HUT_NATIVE_IMPORT ' + json.dumps(report, ensure_ascii=False))
    return report


if __name__ == '__main__':
    import_model()
    complete_materials()
