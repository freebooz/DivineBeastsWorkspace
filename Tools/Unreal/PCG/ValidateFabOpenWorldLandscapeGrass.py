# -*- coding: utf-8 -*-
"""Monolith 内核对 Fab 花草资源及统一风节点；不修改地图或保存资产。"""
import unreal as u

ROOT = '/DBAFoliagePack_Core/Foliage/Grass/FabOpenWorld/'
SOURCE = '/Game/PN_OpenWorldFoliage/Meshes/'
MAT = ROOT + 'M_DBA_OpenWorldFoliage_GlobalWind'
MI = ROOT + 'MI_DBA_OpenWorldFoliage_GlobalWind'
WIND = '/GamePlatformSurface/MaterialFunctions/Vegetation/MF_GP_DirectionalVegetationWind'
INTERACTION = '/Game/PN_OpenWorldFoliage/Materials/Interaction.Interaction'


def run():
    material, instance = u.load_asset(MAT), u.load_asset(MI)
    assert isinstance(material, u.Material) and isinstance(instance, u.MaterialInstanceConstant)
    assert instance.get_editor_property('parent').get_path_name().split('.')[0] == MAT
    lib = u.MaterialEditingLibrary
    wpo = lib.get_material_property_input_node(material, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    assert isinstance(wpo, u.MaterialExpressionAdd)
    branches = lib.get_inputs_for_material_expression(material, wpo)
    assert len(branches) == 2 and all(isinstance(n, u.MaterialExpressionMaterialFunctionCall) for n in branches)
    assert branches[0].get_editor_property('material_function').get_path_name().split('.')[0] == WIND
    assert branches[1].get_editor_property('material_function').get_path_name() == INTERACTION
    for name, prefix, count, density in [
        ('LGT_DBA_OpenWorldGrass', 'openWorld_grass_', 24, 5.),
        ('LGT_DBA_OpenWorldFlowers', 'openWorld_flower_', 12, .5),
    ]:
        asset = u.load_asset(ROOT + name)
        assert isinstance(asset, u.LandscapeGrassType)
        varieties = list(asset.get_editor_property('grass_varieties'))
        assert len(varieties) == count
        seen = set()
        for item in varieties:
            mesh = item.get_editor_property('grass_mesh')
            assert mesh and mesh.get_path_name().startswith(SOURCE + prefix)
            seen.add(mesh.get_path_name())
            mats = item.get_editor_property('override_materials')
            assert len(mats) == 1 and mats[0].get_path_name().split('.')[0] == MI
            assert item.get_editor_property('grass_density').get_editor_property('default') == density
            assert item.get_editor_property('grass_density_quality').get_editor_property('default') == density
            assert item.get_editor_property('start_cull_distance').get_editor_property('default') == 3000
            assert item.get_editor_property('start_cull_distance_quality').get_editor_property('default') == 3000
            assert item.get_editor_property('end_cull_distance').get_editor_property('default') == 4500
            assert item.get_editor_property('end_cull_distance_quality').get_editor_property('default') == 4500
        assert len(seen) == count
        print('FAB_LANDSCAPE_GRASS_ASSET_OK', name, count)
    print('FAB_LANDSCAPE_GRASS_CHECK: native_assets_only; village_binding=false; interaction_runtime=false; cook=false; visual=false')


run()
