#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""导入去重翼面网格，显式保留顶点RGB羽色与A动画权重。

原导入走Interchange并未保留期望的FbxImportUI顶点颜色合同。本次仅在独立作者
进程选择引擎内置FBX导入器，不修改插件/项目配置；新资源_Flight不覆盖原_Cm。
导入后检查实际ImportData类型、顶点色策略、尺寸和材质，再保存真实uasset。
"""
from pathlib import Path
import hashlib
import json
import unreal

ROOT=Path(__file__).resolve().parents[4]
SRC=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock/RefinedMeshes'
DEST='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Meshes/Flight'
MATERIAL='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Materials'


def main():
    if not unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'):
        raise RuntimeError('不是授权神兽联盟工作空间')
    data=json.loads((SRC/'FlightMeshManifest.json').read_text(encoding='utf-8'))
    for row in data['meshes']:
        if hashlib.sha256((SRC/row['file']).read_bytes()).hexdigest()!=row['sha256']:
            raise RuntimeError('修正源网格哈希不符')
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    if not assets.does_directory_exist(DEST):
        assets.make_directory(DEST)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    # 官方FBX管线选择仅影响本次进程；不禁用Niagara或游戏模块，不写全局配置。
    unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    report=[]
    for row in data['meshes']:
        species=row['species'];name='SM_DBA_Bird_'+species
        path=DEST+'/'+name
        needs_import=not assets.does_asset_exist(path)
        if not needs_import:
            mesh=assets.load_asset(path)
            previous=mesh.get_editor_property('asset_import_data')
            if not isinstance(previous,unreal.FbxStaticMeshImportData):
                raise RuntimeError('不覆盖非本工具FBX来源的既有_Flight资源')
            previous_file=str(previous.get_first_filename()).replace('\\','/').lower()
            if previous_file!=str(SRC/row['file']).replace('\\','/').lower():
                raise RuntimeError('不覆盖源文件不匹配的_Flight资源')
            needs_import=not previous.get_editor_property('convert_scene_unit')
        if needs_import:
            settings=unreal.FbxImportUI()
            settings.set_editor_property('import_mesh',True)
            settings.set_editor_property('import_as_skeletal',False)
            settings.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
            settings.set_editor_property('import_materials',False)
            settings.set_editor_property('import_textures',False)
            options=settings.get_editor_property('static_mesh_import_data')
            options.set_editor_property('combine_meshes',True)
            options.set_editor_property('import_uniform_scale',1.0)
            # FBX声明米制；传统导入器默认不转换单位，必须显式转换成UE厘米。
            options.set_editor_property('convert_scene_unit',True)
            options.set_editor_property('convert_scene',True)
            options.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
            options.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            options.set_editor_property('auto_generate_collision',False)
            task=unreal.AssetImportTask()
            task.set_editor_property('filename',str(SRC/row['file']))
            task.set_editor_property('destination_path',DEST)
            task.set_editor_property('destination_name',name)
            task.set_editor_property('factory',unreal.FbxFactory())
            task.set_editor_property('options',settings)
            task.set_editor_property('automated',True)
            # 仅允许修复已核对源文件身份、单位配置错误的本任务_Flight资产。
            task.set_editor_property('replace_existing',assets.does_asset_exist(path))
            task.set_editor_property('save',False)
            tools.import_asset_tasks([task])
            mesh=assets.load_asset(path)
            if not isinstance(mesh,unreal.StaticMesh):
                raise RuntimeError('修正飞行网格未成功导入：'+path)
            material=assets.load_asset(MATERIAL+'/MI_DBA_Bird_'+species)
            if not isinstance(material,unreal.MaterialInstanceConstant):
                raise RuntimeError('既有项目鸟种材质丢失')
            mesh.set_material(0,material)
            if not assets.save_loaded_asset(mesh,only_if_is_dirty=False):
                raise RuntimeError('修正网格保存失败')
        importer=mesh.get_editor_property('asset_import_data')
        if not isinstance(importer,unreal.FbxStaticMeshImportData):
            raise RuntimeError('没有使用明确的FBX顶点颜色导入合同：'+str(type(importer)))
        if importer.get_editor_property('vertex_color_import_option')!=unreal.VertexColorImportOption.REPLACE:
            raise RuntimeError('未导入源顶点颜色与动画权重')
        extent=mesh.get_bounds().box_extent
        if not 30<=max(abs(extent.x),abs(extent.y),abs(extent.z))<=300:
            raise RuntimeError('修正网格厘米单位异常：extent='+str(extent)+' convert_scene_unit='+str(importer.get_editor_property('convert_scene_unit'))+' uniform_scale='+str(importer.get_editor_property('import_uniform_scale')))
        report.append({'species':species,'asset':path,'importer':importer.get_class().get_name(),
            'vertexColor':'ReplaceSourceRGBA','maxExtentCm':max(extent.x,extent.y,extent.z),
            'triangles':row['faces'],'sourceSha256':row['sha256']})
        print('BIRD_FLIGHT_UE_MESH_IMPORTED',json.dumps(report[-1]))
    out=ROOT/'Saved/Validation/BirdFlockP4P7';out.mkdir(parents=True,exist_ok=True)
    (out/'FlightMeshImport.json').write_text(json.dumps({'status':'passed','meshes':report},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_FLIGHT_IMPORT_PASSED',len(report))


if __name__=='__main__':
    main()
