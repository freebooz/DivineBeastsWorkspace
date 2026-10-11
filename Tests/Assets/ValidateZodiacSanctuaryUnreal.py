# -*- coding: utf-8 -*-
"""只读验证 UE 保存圣境资产、共用工作室及真实 PBR 节点闭包。

由正式 UE5.8 编辑器通过 Monolith 执行；不启动登录，不修改 Widget 或权威状态。
网格材质占位、法线色彩空间错误、舞台/锚点丢失均为失败；不是运行或 Cook 验收。
"""
import json
from pathlib import Path
import unreal


def validate_pbr_connections(material,attributes,record,root):
    """核对真实输入闭包、引用和ORM通道，资源存在不能替代已接入着色输出。"""
    lib=unreal.MaterialEditingLibrary
    def inputs(node):
        return dict(zip(lib.get_material_expression_input_names(node),lib.get_inputs_for_material_expression(material,node)))
    def required(node,key):
        value=inputs(node).get(key)
        assert value, 'PBR '+key+' 输出断链：'+material.get_name()
        return value
    def parameter(node,name):
        assert node.get_editor_property('parameter_name')==unreal.Name(name), '参数接错：'+name
    def texture(node,key):
        assert isinstance(node,unreal.MaterialExpressionTextureSample), '不是纹理采样：'+key
        expected=root+'/Textures/'+Path(record[key]).stem
        assert node.get_editor_property('texture').get_path_name().split('.')[0]==expected, 'PBR纹理接错：'+key
    base=required(attributes,'BaseColor')
    if record.get('texture'):
        texture(required(base,'A'),'texture');parameter(required(base,'B'),'SurfaceTint')
    else:
        assert base, 'BaseColor缺失'
    if record.get('normal_texture'):
        texture(required(attributes,'Normal'),'normal_texture')
    roughness=required(attributes,'Roughness')
    if record.get('orm_texture'):
        control=required(roughness,'A');parameter(required(roughness,'B'),'SurfaceRoughness')
        orm=required(control,'A');texture(orm,'orm_texture')
        assert lib.get_input_node_output_name_for_material_expression(control,orm)=='G', 'ORM粗糙度必须接G'
        ao=required(attributes,'AmbientOcclusion');texture(ao,'orm_texture')
        assert lib.get_input_node_output_name_for_material_expression(attributes,ao)=='R', 'ORM遮蔽必须接R'
    else:
        parameter(roughness,'SurfaceRoughness')
    # 本期金属度由可审核参数拥有；B通道保留原始数据，不声称消费未接入通道。
    parameter(required(attributes,'Metallic'),'MetallicStrength')
    if record.get('coat_weight',0):
        parameter(required(attributes,'ClearCoat'),'PolishCoatWeight')
        parameter(required(attributes,'ClearCoatRoughness'),'PolishCoatRoughness')


def main():
    """核对重新加载的保存件，而非只检查源 FBX 文件存在。"""
    source = Path(unreal.Paths.project_dir()).resolve() / 'Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/SourceArt/Blender/ZodiacSanctuary'
    manifest = json.loads((source / 'SceneManifest.json').read_text(encoding='utf-8'))
    root = '/DBAFrontEndPack/CharacterPreview/Environment/ZodiacSanctuary'
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split('.')[0] == '/DBAFrontEndPack/Maps/L_DBA_CharacterStudio', '须重新打开正式工作室'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    stages = [a for a in actors if a.get_class().get_name() == 'GamePlatformCharacterPreviewStage']
    assert len(stages) == 1, '共用平台舞台必须唯一'
    environment = [a for a in actors if unreal.Name('DBA.Preview.ZodiacSanctuary') in a.tags]
    assert len(environment) == len(manifest['objects']), '地图圣境网格实例不完整'
    for actor in environment:
        rotation=actor.get_actor_rotation()
        assert abs(rotation.pitch)<.01 and abs(rotation.roll)<.01 and abs(rotation.yaw+90)<.01, '场景轴转换错误'
    camera=stages[0].get_component_by_class(unreal.CameraComponent)
    rotation=camera.get_world_rotation()
    assert abs(rotation.roll)<.01 and abs(rotation.pitch+8)<.01, '预览镜头翻转或俯仰错误'
    assert any(unreal.Name('DBA.Foliage.Ground') in a.tags for a in actors), '自然飘落锚点被破坏'
    for record in manifest['objects']:
        mesh = unreal.load_asset(root + '/Meshes/' + record['name'])
        assert isinstance(mesh, unreal.StaticMesh) and mesh.get_bounds().sphere_radius > 0, '真实网格缺失'
        assert all(slot.material_interface and slot.material_interface.get_path_name().startswith(root + '/Materials/') for slot in mesh.static_materials), '网格仍引用占位材质：' + record['name']
    texture_paths = set()
    for record in manifest['materials']:
        material = unreal.load_asset(root + '/Materials/' + record['name'])
        assert material and material.get_editor_property('use_material_attributes'), 'PBR 材质不存在或属性节点未启用'
        nodes = unreal.MaterialEditingLibrary.get_material_expressions(material)
        attributes = [n for n in nodes if isinstance(n, unreal.MaterialExpressionMakeMaterialAttributes)]
        assert len(attributes) == 1 and unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES) == attributes[0], '真实 PBR 输出未连接'
        validate_pbr_connections(material,attributes[0],record,root)
        for node in nodes:
            if isinstance(node, unreal.MaterialExpressionTextureSample):
                assert node.get_editor_property('texture'), '材质存在空纹理采样'
        for key in ('texture', 'normal_texture', 'orm_texture'):
            if not record.get(key):
                continue
            path = root + '/Textures/' + Path(record[key]).stem
            texture = unreal.load_asset(path); texture_paths.add(path)
            assert texture, '增强纹理缺失：' + path
            assert texture.get_editor_property('srgb') == (key == 'texture'), '色彩空间不正确：' + path
            if key == 'normal_texture':
                assert texture.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_NORMALMAP and texture.get_editor_property('flip_green_channel'), '切线法线约定未转换'
        if record.get('coat_weight', 0):
            assert material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_CLEAR_COAT, '抛光层未启用'
    result = {'meshes': len(manifest['objects']), 'materials': len(manifest['materials']), 'textures': len(texture_paths), 'stage_class': stages[0].get_class().get_name(), 'map_reloaded': True}
    print('DBA_SANCTUARY_UNREAL_ASSETS_OK ' + json.dumps(result))


if __name__ == '__main__':
    main()
