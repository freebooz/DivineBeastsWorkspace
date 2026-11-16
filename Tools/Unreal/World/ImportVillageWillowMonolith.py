# -*- coding: utf-8 -*-
"""Monolith原生导入用户柳树，植物资产归共享植物包，世界只拥有摆放。"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFoliagePack_Core'
DEST = '/DBAFoliagePack_Core/Foliage/Trees/WeepingWillow'
MESH = DEST + '/SM_DBA_Foliage_WeepingWillow_Med'


def import_model():
    """保留原尺度和全部LOD；只首次导入，不覆盖已有网格。"""
    lib = unreal.EditorAssetLibrary
    assert not lib.does_asset_exist(MESH), '禁止覆盖已有柳树资产'
    options = unreal.SpeedTreeImportData()
    options.set_editor_property('tree_scale', 1.0)
    options.set_editor_property('import_geometry_type', unreal.ImportGeometryType.IGT_3D)
    options.set_editor_property('include_collision', True)
    options.set_editor_property('make_materials_check', True)
    options.set_editor_property('include_normal_map_check', True)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(PACK / 'SourceArt/Trees/WeepingWillow_Med/WeepingWillow_Med.st9'))
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', MESH.rsplit('/', 1)[1])
    task.set_editor_property('factory', unreal.SpeedTreeImportFactory())
    task.set_editor_property('options', options)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', False)
    task.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = lib.load_asset(MESH)
    assert isinstance(mesh, unreal.StaticMesh)
    report = {'mesh': mesh.get_path_name(), 'lod_count': mesh.get_num_lods(),
              'bounds_origin': str(mesh.get_bounds().origin), 'bounds_extent': str(mesh.get_bounds().box_extent),
              'materials': [str(x.material_interface) for x in mesh.get_editor_property('static_materials')]}
    print('WILLOW_NATIVE_IMPORT ' + json.dumps(report))
    return report


def finalize():
    """接齐原图，保留SpeedTree9法线RGB与粗糙度Alpha合同，再原生规范命名、保存。"""
    lib = unreal.EditorAssetLibrary
    aliases = {'Branches_02': 'Branches', 'Cap_01': 'Cap', 'FrondStem': 'FrondStem',
               'WeepingWillowBark': 'Bark', 'WeepingWillow_Med': 'Atlas'}
    assets = [lib.load_asset(str(p)) for p in lib.list_assets(DEST, True, False)]
    for asset in assets:
        if isinstance(asset, unreal.Texture2D):
            original = asset.get_name()
            kind = next((k for k in ['Color', 'Normal', 'Subsurface'] if '_' + k in original), None)
            assert kind, original
            alias = next((v for k, v in aliases.items() if original.startswith(k + '_')), None)
            assert alias, original
            asset.set_editor_property('srgb', kind != 'Normal')
            asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
            asset.set_editor_property('adjust_min_alpha', 0.0)
            target = DEST + '/T_DBA_Willow_' + alias + '_' + kind
        elif isinstance(asset, unreal.MaterialInstanceConstant):
            original = asset.get_name()
            alias = next((v for k, v in aliases.items() if original == k + '_Mat'), None)
            assert alias, original
            target = DEST + '/MI_DBA_Willow_' + alias
        else:
            continue
        assert lib.rename_asset(asset.get_path_name(), target)
        assert lib.save_loaded_asset(asset, False)
    mesh = lib.load_asset(MESH)
    assert lib.save_loaded_asset(mesh, False)
    print('WILLOW_FINALIZED', len(assets))


def bind_materials():
    """显式连接原图；本机原生导入虽导入法线，却遗留EmptyNormal默认参数。"""
    for alias in ['Branches', 'Cap', 'FrondStem', 'Bark', 'Atlas']:
        material = unreal.load_asset(DEST + '/MI_DBA_Willow_' + alias)
        for suffix, parameter in [('Color', 'ColorOpacity'), ('Normal', 'NormalRoughness')]:
            texture = unreal.load_asset(DEST + '/T_DBA_Willow_' + alias + '_' + suffix)
            assert isinstance(texture, unreal.Texture2D)
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, parameter, texture)
        if alias == 'Atlas':
            texture = unreal.load_asset(DEST + '/T_DBA_Willow_Atlas_Subsurface')
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, 'Subsurface', texture)
        unreal.MaterialEditingLibrary.update_material_instance(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    print('WILLOW_MATERIAL_BINDINGS_SAVED', 5)


if __name__ == '__main__':
    import_model()
    finalize()
    bind_materials()
