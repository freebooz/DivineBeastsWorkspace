# -*- coding: utf-8 -*-
"""在UE编辑器用独占开发测试材质验证导入拒绝覆盖未保存修改。

不修改正式材质、地图或用户资产；测试临时材质仅归随机UUID开发路径，结束精确清理。
旧实现会保存临时材质并失败；修复实现必须在节点删除与保存之前拒绝操作。
"""
import gc
import importlib.util
from pathlib import Path
import uuid
import unreal


def main():
    """真实脏包回归，不用模拟列表冒充编辑器未保存状态。"""
    script=Path(unreal.Paths.project_dir()).resolve().parent/'Tools/Unreal/FrontEnd/ImportZodiacSanctuary.py'
    spec=importlib.util.spec_from_file_location('sanctuary_import_safety',script)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    module.require_editor()
    module.ROOT='/Game/Development/ZodiacSanctuarySafety/'+uuid.uuid4().hex
    name='M_DBA_Sanctuary_DirtyProbe'
    path=module.ROOT+'/Materials/'+name
    probe=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,module.ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    assert probe, '测试材质创建失败'
    try:
        # 工厂新建不保证每种编辑器事务状态都标脏，显式模拟一次真实未保存编辑。
        probe.modify(True)
        probe.set_editor_property('two_sided',True)
        assert probe.get_outer() in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '测试未产生真实脏包'
        before=len(unreal.MaterialEditingLibrary.get_material_expressions(probe))
        record=dict(module.MANIFEST['materials'][0],name=name,texture=None,normal_texture=None,orm_texture=None,coat_weight=0)
        refused=False
        try:
            module.create_material(record)
        except AssertionError as error:
            assert '未保存' in str(error), '拒绝原因不是未保存保护：'+str(error)
            refused=True
        assert refused, '导入覆盖并保存了未保存的材质修改'
        assert len(unreal.MaterialEditingLibrary.get_material_expressions(probe))==before, '拒绝前已改变节点'
    finally:
        # 只删除本次创建的唯一测试对象；从不扫描或删除整个开发目录。
        assert path.startswith(module.ROOT+'/Materials/'), '临时材质目标越界'
        # Python包装器持有原生GC引用；先释放再按身份删除，避免DeleteLoadedAsset拒绝。
        probe=None
        gc.collect()
        assert unreal.EditorAssetLibrary.delete_asset(path), '独占测试材质未成功清理，不能报告完整通过'
        assert not unreal.EditorAssetLibrary.does_asset_exist(path), '测试材质仍残留于资产注册表'
        assert path not in [item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()], '测试脏包仍未清理'
    print('SANCTUARY_UNSAVED_MATERIAL_PROTECTION_OK')


if __name__=='__main__':
    main()
