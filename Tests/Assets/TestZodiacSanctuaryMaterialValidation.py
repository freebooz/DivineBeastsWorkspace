# -*- coding: utf-8 -*-
"""用真实UE材质副本证明资产门禁能够发现法线输出断链。

只断开随机独占开发副本，不修改正式资源；加载适配器仅将一个输入身份指向真实副本，
其余引擎调用及纹理/世界均为真实对象。结束精确删除唯一临时资产，不清理用户目录。
"""
import gc
import importlib.util
from pathlib import Path
import uuid
import unreal


def main():
    """保留法线纹理却断开Normal输入，旧门禁会误报成功，修复后必须失败。"""
    # 环境前置检查先于临时资源创建，避免错误地图或PIE触发无关失败。
    assert Path(unreal.Paths.get_project_file_path()).name=='DivineBeastsArena.uproject', '须使用正式神兽联盟工程'
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), '须先结束PIE再验证保存资产'
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split('.')[0]=='/DBAFrontEndPack/Maps/L_DBA_CharacterStudio', '须重新打开正式工作室'
    directory=Path(unreal.Paths.project_dir()).resolve().parent/'Tests/Assets'
    spec=importlib.util.spec_from_file_location('sanctuary_validation_probe',directory/'ValidateZodiacSanctuaryUnreal.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    original='/DBAFrontEndPack/CharacterPreview/Environment/ZodiacSanctuary/Materials/M_DBA_Sanctuary_Stone'
    destination='/Game/Development/ZodiacSanctuarySafety/'+uuid.uuid4().hex+'/M_NormalDisconnectProbe'
    probe=unreal.EditorAssetLibrary.duplicate_asset(original,destination)
    assert probe, '测试真实材质复制失败'
    attributes=unreal.MaterialEditingLibrary.get_material_property_input_node(probe,unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
    assert unreal.MaterialEditingLibrary.disconnect_material_expressions(attributes,'Normal'), '测试未断开真实Normal输出'

    class AssetInputAdapter:
        """只选择一个真实测试输入，不替代引擎材质数据或验证行为。"""
        def __getattr__(self,name):
            return getattr(unreal,name)
        def load_asset(self,path,*args,**kwargs):
            return probe if path==original else unreal.load_asset(path,*args,**kwargs)

    module.unreal=AssetInputAdapter()
    refused=False
    try:
        try:
            module.main()
        except AssertionError as error:
            assert 'Normal' in str(error), '失败原因不是法线断链：'+str(error)
            refused=True
        assert refused, '法线输出已经断链但门禁仍通过'
    finally:
        assert destination.startswith('/Game/Development/ZodiacSanctuarySafety/'), '测试清理目标越界'
        # 适配器、材质及节点的Python包装器均会阻止原生删除，必须先释放真实引用。
        module.unreal=unreal
        probe=None
        attributes=None
        gc.collect()
        assert unreal.EditorAssetLibrary.delete_asset(destination), '独占法线测试材质未成功清理，不能报告完整通过'
        assert not unreal.EditorAssetLibrary.does_asset_exist(destination), '法线测试材质仍残留于资产注册表'
        assert destination not in [item.get_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()], '法线测试脏包仍未清理'
    print('SANCTUARY_DISCONNECTED_NORMAL_REJECTED_OK')


if __name__=='__main__':
    main()
