#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""补存FBX导入的真实狼子资产；只通过本轮自有编辑器的Monolith执行原生导入/保存。

主网格保存不代表骨架、动画、材质已落盘。本工具使用同一CC0源FBX与稳定路径，
完整保存五项依赖并恢复已有混合空间与AnimBP引用；不修改服务器实体或创建第二套资源。
"""
from pathlib import Path
import importlib.util
import json

ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = ROOT / 'Saved/Validation/VillageWolvesAndPartyUI20261016'
spec = importlib.util.spec_from_file_location('fauna_protocol', ROOT / 'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)
protocol.EVIDENCE = EVIDENCE / 'Calls'


def main():
    """依赖必须真实保存并可从磁盘查到，失败保留日志、不自动重复导入。"""
    pid = json.loads((EVIDENCE/'EditorRepair.json').read_text(encoding='utf-8-sig'))['PID']
    source = (ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Creatures/Wolf/Wolf.fbx').as_posix()
    result = protocol.editor_python(f'''
import unreal,os
assert os.getpid()=={pid}
assert '-UIWolfRepairOwned' in unreal.SystemLibrary.get_command_line()
ui=unreal.FbxImportUI()
ui.set_editor_property('import_as_skeletal',True)
ui.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property('automated_import_should_detect_type',False)
ui.set_editor_property('import_animations',True)
ui.set_editor_property('import_materials',True)
ui.set_editor_property('import_textures',False)
ui.set_editor_property('create_physics_asset',False)
task=unreal.AssetImportTask()
for k,v in {{'filename':{source!r},'destination_path':'/DBAWorldPack_Village/Creatures/Wolf','destination_name':'SK_DBA_VillageWolf','automated':True,'replace_existing':True,'save':True,'options':ui,'factory':unreal.FbxFactory()}}.items(): task.set_editor_property(k,v)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
base='/DBAWorldPack_Village/Creatures/Wolf/'
names=['SK_DBA_VillageWolf','SK_DBA_VillageWolf_Skeleton','SK_DBA_VillageWolf_Anim_WolfArmature_Idle','SK_DBA_VillageWolf_Anim_WolfArmature_Walking','Wolf']
# FBX重导入既有网格不会补建缺失动画，必须显式走动画导入并逐项保存所有返回子资产。
if not unreal.EditorAssetLibrary.does_asset_exist(base+names[2]) or not unreal.EditorAssetLibrary.does_asset_exist(base+names[3]):
    ui.set_editor_property('import_mesh',False)
    ui.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('skeleton',unreal.load_asset(base+names[1]))
    task.set_editor_property('destination_name','SK_DBA_VillageWolf_Anim')
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
# 原始FBX为无贴图灰色材质；缺失时在引擎中恢复同名不透明灰材质，避免空槽显示异常。
material=unreal.load_asset(base+'Wolf')
if not material:
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('Wolf',base[:-1],unreal.Material,unreal.MaterialFactoryNew())
    color=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant',unreal.LinearColor(0.8,0.8,0.8,1.0))
    unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant)
    rough.set_editor_property('r',0.8)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
mesh=unreal.load_asset(base+names[0]); slots=mesh.get_editor_property('materials')
for slot in slots: slot.set_editor_property('material_interface',material)
mesh.set_editor_property('materials',slots)
for name in names:
    a=unreal.load_asset(base+name)
    assert a,name
    assert unreal.EditorAssetLibrary.save_loaded_asset(a,False),name
    print('DEPENDENCY_SAVED',a.get_path_name())
# 依赖身份不变；从已保存包重载引用，而不写只读Skeleton属性或生成第二个骨架。
bs=unreal.load_asset(base+'BS_DBA_VillageWolf')
bp=unreal.load_asset(base+'ABP_DBA_VillageWolf')
ok,reason=unreal.EditorLoadingAndSavingUtils.reload_packages([bs.get_outermost(),bp.get_outermost()])
assert ok,str(reason)
bs=unreal.load_asset(base+'BS_DBA_VillageWolf');bp=unreal.load_asset(base+'ABP_DBA_VillageWolf')
assert bs.get_editor_property('skeleton')
assert all(s.get_editor_property('animation') for s in bs.get_editor_property('sample_data'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
print('IMPORTED_WOLF_DEPENDENCIES_PERSISTED')
''')
    (EVIDENCE/'SavedDependencies.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('WOLF_IMPORT_DEPENDENCIES_SAVED')


if __name__ == '__main__':
    main()
