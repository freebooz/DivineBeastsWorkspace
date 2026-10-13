# -*- coding: utf-8 -*-
"""Monolith内真实草地资产门禁：检查客户端扫描贴图及共享材质参数，不以PNG存在代替UE导入。
验证目标：缺贴图、错误Gamma/压缩、共享材质硬引用客户端资源和出生点变更必须失败。
只读正式资产；输出Saved证据，不保存地图。成功不代表Cook、网络或人工美术验收。
"""
import json
from pathlib import Path
import unreal


def validate():
    """独立固定合同；资源必须是真实2KTexture2D，材质默认仅引用共享世界包。"""
    root = '/DBAPresentationPack_Core/Textures/Terrain/Grass/'
    textures = {}
    for role, srgb, compression in (('BaseColor', True, unreal.TextureCompressionSettings.TC_DEFAULT),
                                    ('Normal', False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                                    ('ORM', False, unreal.TextureCompressionSettings.TC_MASKS)):
        path = root + 'T_DBA_Village_Grass_' + role
        tex = unreal.load_asset(path)
        assert isinstance(tex, unreal.Texture2D), f'尚无真实草地Texture2D: {path}'
        assert tex.get_editor_property('srgb') == srgb, f'草地Gamma错误: {role}'
        assert tex.get_editor_property('compression_settings') == compression, f'草地压缩错误: {role}'
        assert tex.blueprint_get_size_x() == 2048 and tex.blueprint_get_size_y() == 2048, f'不是真实2K: {role}'
        textures[role] = path
    material = unreal.load_asset('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
    lib = unreal.MaterialEditingLibrary
    expressions = list(lib.get_material_expressions(material))
    assert len(expressions) == 55, '材质存在残留/重复/缺失节点，需人工核对'
    def at(cls, x, y):
        nodes = [n for n in expressions if isinstance(n, cls) and
                 n.get_editor_property('material_expression_editor_x') == x and
                 n.get_editor_property('material_expression_editor_y') == y]
        assert len(nodes) == 1, f'节点位置不唯一: {x}/{y}'
        return nodes[0]
    scale = at(unreal.MaterialExpressionScalarParameter, -2200, 3200)
    assert str(scale.get_editor_property('parameter_name')) == 'VillageGroundUVScale'
    assert abs(scale.get_editor_property('default_value') - .004) < 1e-7, '草地不是250厘米重复'
    uv = at(unreal.MaterialExpressionMultiply, -1940, 3200)
    xy = at(unreal.MaterialExpressionComponentMask, -2440, -1070)
    assert list(lib.get_inputs_for_material_expression(material, uv)) == [xy, scale], '世界坐标/缩放未接通'
    samples = []
    for name in ('VillageGroundAlbedoTexture', 'VillageGroundNormalTexture', 'VillageGroundORMTexture'):
        nodes = [n for n in expressions if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D)
                 and str(n.get_editor_property('parameter_name')) == name]
        assert len(nodes) == 1, '草地纹理参数重复或缺失'
        assert lib.get_inputs_for_material_expression(material, nodes[0])[0] == uv, '草地UV未接通'
        samples.append(nodes[0])
        default = lib.get_material_default_texture_parameter_value(material, name)
        assert default and default.get_path_name().startswith('/DBAWorldPack_Village/'), f'共享草地默认不合规: {name}'
    color = at(unreal.MaterialExpressionMultiply, -600, 3200)
    rough = at(unreal.MaterialExpressionMultiply, -600, 3500)
    assert list(lib.get_inputs_for_material_expression(material, color)) == [samples[0], at(unreal.MaterialExpressionLinearInterpolate, -980, 1630)]
    assert list(lib.get_inputs_for_material_expression(material, rough)) == [samples[2], at(unreal.MaterialExpressionLinearInterpolate, -800, 2130)]
    assert lib.get_input_node_output_name_for_material_expression(rough, samples[2]) == 'G', 'ORM粗糙度未使用G通道'
    assert lib.get_inputs_for_material_expression(material, at(unreal.MaterialExpressionLinearInterpolate, 370, 400))[0] == color
    assert lib.get_inputs_for_material_expression(material, at(unreal.MaterialExpressionLinearInterpolate, 370, 1940))[0] == rough
    normals = [n for n in expressions if isinstance(n, unreal.MaterialExpressionLinearInterpolate)
               and n.get_editor_property('material_expression_editor_y') > 2500]
    assert len(normals) == 1 and lib.get_inputs_for_material_expression(material, normals[0])[0] == samples[1]
    assert lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION) == samples[2]
    assert lib.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION) == 'R', 'ORM遮蔽未使用R通道'
    errors = list(lib.recompile_material(material))
    assert not errors, f'草地材质编译失败: {errors}'
    # 查整个共享材质的递归硬依赖，不只看三张参数的默认贴图。
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False,
        include_hard_package_references=True, include_searchable_names=False,
        include_soft_management_references=False, include_hard_management_references=False)
    pending = [material.get_outermost().get_name()]
    visited = set()
    forbidden = ('/GamePlatformSurface/', '/GamePlatformVFX/', '/DBAPresentationPack_Core/')
    while pending:
        package = str(pending.pop())
        if package in visited:
            continue
        visited.add(package)
        assert not package.startswith(forbidden), f'共享材质存在客户端硬依赖: {package}'
        pending.extend(registry.get_dependencies(package, options))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_outermost().get_name() == '/DBAWorldPack_Village/Maps/L_Village_Start', '未审查正式新手村'
    starts = sorted((a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                     if isinstance(a, unreal.PlayerStart)), key=lambda a: a.get_name())
    assert len(starts) == 2, '出生点数量改变'
    for start, x in zip(starts, (0., 240.)):
        p, r, s = start.get_actor_location(), start.get_actor_rotation(), start.get_actor_scale3d()
        assert (p.x, p.y, p.z, r.pitch, r.yaw, r.roll, s.x, s.y, s.z) == (x, 0., 120., 0., 0., 0., 1., 1., 1.), '出生点被改动'
    return {'Passed': True, 'Textures': textures, 'ProtectedPlayerStarts': 2,
            'RuntimeVerified': False, 'CookVerified': False, 'ManualVisualAccepted': False}


result = validate()
output = Path(unreal.Paths.project_dir()).parent / 'Saved/Validation/VillageAmbient20261013/GrassAssets.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print('VILLAGE_GRASS_ASSETS_OK ' + json.dumps(result, ensure_ascii=False))
