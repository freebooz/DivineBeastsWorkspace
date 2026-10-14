# -*- coding: utf-8 -*-
"""Monolith内的新手村草地增量作者。项目层客户端持有三张真实扫描PBR，复用现有共享地形桥。
不重建地图/地形、碰撞/导航，不修改PlayerStart，不覆盖历史十层审核或旧纹理。
共享材质仅使用既有中性默认纹理，高清草地由Data世界租约写入MID；生成前保存精确回退。
只在本工具明确路径写真实引擎资源。输出Saved证据；成功不代表Cook、联机或人工审美通过。
"""
import hashlib
import json
import shutil
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).parent
OUT = ROOT / 'Saved/Validation/VillageAmbient20261013'
MASTER = '/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
DEST = '/DBAPresentationPack_Core/Textures/Terrain/Grass'
ROLLBACK = None
CREATED_TEXTURES = []


def author():
    """先核对正式工程、来源哈希和脏包，再导入新纹理并增量连接共享材质的原地面分支。"""
    global ROLLBACK
    assert unreal.Paths.get_project_file_path().replace('\\', '/').endswith('/Game/DivineBeastsArena.uproject')
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    assert not dirty, '存在未保存资产，拒绝覆盖'
    mat = unreal.load_asset(MASTER)
    lib = unreal.MaterialEditingLibrary
    existing = list(lib.get_material_expressions(mat))
    names = [str(n.get_editor_property('parameter_name')) for n in existing
             if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D)
             and str(n.get_editor_property('parameter_name')).startswith('VillageGround')]
    if names:
        # 先运行完整真实资产/拓扑门禁；已完成仅只读校验，不追加节点或重导入。
        assert sorted(names) == ['VillageGroundAlbedoTexture', 'VillageGroundNormalTexture', 'VillageGroundORMTexture']
        assert len(existing) == 55, '已有不完整/重复草地图，拒绝猜测修复'
        exec(compile((ROOT / 'Tests/Assets/ValidateVillageGrassAssetsMonolith.py').read_text(encoding='utf-8'),
                     'ValidateVillageGrassAssetsMonolith.py', 'exec'), {})
        print('VILLAGE_GRASS_ALREADY_COMPLETE_VERIFIED')
        return
    assert len(existing) == 48, '初始材质不是批准的48节点版本，拒绝创建'
    OUT.mkdir(parents=True, exist_ok=True)
    backup = OUT / 'LandscapeBridge.before-grass.uasset'
    src = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset'
    if not backup.exists():
        shutil.copy2(src, backup)
    records = json.loads((ROOT / 'Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json').read_text(encoding='utf-8'))['files']
    settings = {'BaseColor': (True, unreal.TextureCompressionSettings.TC_DEFAULT),
                'Normal': (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                'ORM': (False, unreal.TextureCompressionSettings.TC_MASKS)}
    imported = []
    for role, (srgb, compression) in settings.items():
        rec = next(r for r in records if r['layer'] == 'Grass' and r['role'] == role)
        source = ROOT / rec['sourcePath']
        assert hashlib.sha256(source.read_bytes()).hexdigest() == rec['sha256'], f'扫描源已漂移: {role}'
        name = 'T_DBA_Village_Grass_' + role
        existed = unreal.EditorAssetLibrary.does_asset_exist(DEST + '/' + name)
        task = unreal.AssetImportTask()
        for key, value in {'filename': str(source), 'destination_path': DEST, 'destination_name': name,
                           'automated': True, 'replace_existing': False, 'save': False}.items():
            task.set_editor_property(key, value)
        if not existed:
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex = unreal.load_asset(DEST + '/' + name)
        assert isinstance(tex, unreal.Texture2D), f'导入失败: {role}'
        if existed:
            sources = list(tex.get_editor_property('asset_import_data').extract_filenames())
            assert len(sources) == 1 and Path(sources[0]).resolve() == source.resolve(), '已有纹理来源身份不同，拒绝覆盖'
            assert tex.get_editor_property('srgb') == srgb and tex.get_editor_property('compression_settings') == compression, '已有资产配置漂移，拒绝覆盖'
        else:
            CREATED_TEXTURES.append(tex)
            tex.set_editor_property('srgb', srgb)
            tex.set_editor_property('compression_settings', compression)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
            tex.set_editor_property('address_x', unreal.TextureAddress.TA_WRAP)
            tex.set_editor_property('address_y', unreal.TextureAddress.TA_WRAP)
        assert unreal.EditorAssetLibrary.save_loaded_asset(tex), '纹理保存失败'
        imported.append({'Role': role, 'Asset': tex.get_path_name(), 'SourceSHA256': rec['sha256'], 'License': rec['license']})
    # 用原始表达式坐标定位既有地面/积雪混合输入，不重建其48节点及天气参数合同。
    # 使用锁定UE5.8实际公开的表达式查询，不能访问已隐藏的EditorOnlyData数组。
    expressions = list(lib.get_material_expressions(mat))
    def at(cls, x, y):
        matches = [e for e in expressions if isinstance(e, cls) and e.get_editor_property('material_expression_editor_x') == x and e.get_editor_property('material_expression_editor_y') == y]
        assert len(matches) == 1, f'原材质拓扑不匹配: {cls.__name__} {x}/{y}'
        return matches[0]
    def node(cls, x, y):
        result = lib.create_material_expression(mat, cls, x, y)
        assert result, '创建真实表达式失败'
        ROLLBACK['nodes'].append(result)
        return result
    def wire(a, b, pin, output=''):
        assert lib.connect_material_expressions(a, output, b, pin), f'连接失败: {pin}'
    world_xy = at(unreal.MaterialExpressionComponentMask, -2440, -1070)
    earth = at(unreal.MaterialExpressionLinearInterpolate, -980, 1630)
    final_color = at(unreal.MaterialExpressionLinearInterpolate, 370, 400)
    earth_r = at(unreal.MaterialExpressionLinearInterpolate, -800, 2130)
    final_rough = at(unreal.MaterialExpressionLinearInterpolate, 370, 1940)
    normals = [e for e in expressions if isinstance(e, unreal.MaterialExpressionLinearInterpolate) and e.get_editor_property('material_expression_editor_y') > 2500]
    assert len(normals) == 1, '原法线混合节点不唯一'
    targets = [final_color, final_rough, normals[0]]
    bindings = []
    for target in targets:
        source = lib.get_inputs_for_material_expression(mat, target)[0]
        assert source, '原地面A分支缺失，拒绝修改'
        output = lib.get_input_node_output_name_for_material_expression(target, source)
        assert isinstance(output, str), '无法读取原连接的输出身份'
        bindings.append((target, source, output))
    ao = lib.get_material_property_input_node(mat, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    ao_output = lib.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION) if ao else ''
    ROLLBACK = {'material': mat, 'nodes': [], 'bindings': bindings, 'ao': ao, 'ao_output': ao_output}
    # 地表PBR使用世界XY厘米坐标，默认250cm一重复；不改变几何或位移。
    scale = node(unreal.MaterialExpressionScalarParameter, -2200, 3200)
    scale.set_editor_property('parameter_name', 'VillageGroundUVScale')
    scale.set_editor_property('default_value', .004)
    uv = node(unreal.MaterialExpressionMultiply, -1940, 3200)
    wire(world_xy, uv, 'A'); wire(scale, uv, 'B')
    defaults = '/DBAWorldPack_Village/Materials/Landscape/Defaults/'
    samples = {}
    for index, (role, parameter, default) in enumerate((
            ('BaseColor', 'VillageGroundAlbedoTexture', 'T_DBA_Village_SnowAlbedoDefault'),
            ('Normal', 'VillageGroundNormalTexture', 'T_DBA_Village_SnowNormalDefault'),
            ('ORM', 'VillageGroundORMTexture', 'T_DBA_Village_SnowCoverageDefault'))):
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -1650, 3200 + index * 300)
        sample.set_editor_property('parameter_name', parameter)
        sample.set_editor_property('texture', unreal.load_asset(defaults + default))
        sample.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        sampler = {'BaseColor': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
                   'Normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
                   'ORM': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[role]
        sample.set_editor_property('sampler_type', sampler)
        wire(uv, sample, 'UVs')
        samples[role] = sample
    ground_color = node(unreal.MaterialExpressionMultiply, -600, 3200)
    wire(samples['BaseColor'], ground_color, 'A', 'RGB'); wire(earth, ground_color, 'B')
    wire(ground_color, final_color, 'A')
    ground_rough = node(unreal.MaterialExpressionMultiply, -600, 3500)
    wire(samples['ORM'], ground_rough, 'A', 'G'); wire(earth_r, ground_rough, 'B')
    wire(ground_rough, final_rough, 'A')
    wire(samples['Normal'], normals[0], 'A', 'RGB')
    # ORM使用线性Masks采样；共享中性默认同样为TC_MASKS/non-sRGB，不产生运行时采样类别冲突。
    assert lib.connect_material_property(samples['ORM'], 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    errors = list(lib.recompile_material(mat))
    assert not errors, f'材质编译失败，禁止保存/成功报告；保留脏包供人工恢复: {errors}'
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat), '共享地形材质保存失败'
    payload = {'Imported': imported, 'SharedMaterial': MASTER, 'Rollback': str(backup),
               'MapWritten': False, 'PlayerStartsChanged': False, 'CollisionChanged': False,
               'CookVerified': False, 'ManualVisualAccepted': False}
    (OUT / 'GrassAuthoring.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding='utf-8')
    print('VILLAGE_GRASS_AUTHOR_OK ' + json.dumps(payload, ensure_ascii=False))


try:
    author()
except Exception:
    # 只撤销本轮拥有的表达式与新资源；经引擎接口恢复原连接，不直接覆盖.uasset二进制。
    if ROLLBACK:
        lib = unreal.MaterialEditingLibrary
        mat = ROLLBACK['material']
        for n in ROLLBACK['nodes']:
            lib.delete_material_expression(mat, n)
        for target, source, output in ROLLBACK['bindings']:
            assert lib.connect_material_expressions(source, output, target, 'A'), '回退原分支失败'
        if ROLLBACK['ao']:
            assert lib.connect_material_property(ROLLBACK['ao'], ROLLBACK['ao_output'], unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        else:
            lib.disconnect_material_property(mat, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        assert not list(lib.recompile_material(mat)), '回退母材质编译失败，保留脏包人工处理'
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat), '回退母材质保存失败'
    for texture in CREATED_TEXTURES:
        assert unreal.EditorAssetLibrary.delete_loaded_asset(texture), '本轮新纹理回退失败'
    print('VILLAGE_GRASS_AUTHOR_FAILED_ROLLED_BACK_OWN_MUTATIONS')
    raise
