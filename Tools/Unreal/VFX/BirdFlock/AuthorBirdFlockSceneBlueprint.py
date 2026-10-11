#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8编辑器鸟群场景蓝图作者：Actor承载项目组件，渲染仍经唯一Presentation/VFX链。
仅操作新建的独立鸟群蓝图；已存在资产只验证，不覆盖人工设置。不得用旧ISM父类替代。
"""
import json
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[4]
PATH='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockAmbient'

def main():
    cls=getattr(unreal,'DivineBeastsBirdFlockSceneComponent',None)
    assert cls is not None,'必须先编译并加载鸟群正式场景组件'
    assets=unreal.EditorAssetLibrary
    if assets.does_asset_exist(PATH):
        bp=assets.load_asset(PATH)
        assert isinstance(bp,unreal.Blueprint)
    else:
        dest,name=PATH.rsplit('/',1)
        assets.make_directory(dest)
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.Actor)
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,dest,unreal.Blueprint,factory)
        assert bp
        sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles=sub.k2_gather_subobject_data_for_blueprint(bp)
        assert handles,'未找到蓝图根组件'
        params=unreal.AddNewSubobjectParams(parent_handle=handles[0],new_class=cls,blueprint_context=bp)
        handle,reason=sub.add_new_subobject(params=params)
        assert not str(reason),str(reason)
        assert sub.rename_subobject(handle=handle,new_name=unreal.Text('BirdFlock'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp,only_if_is_dirty=False)
    loaded=assets.load_blueprint_class(PATH)
    assert loaded is not None
    print('BIRD_SCENE_BLUEPRINT_SAVED',PATH,loaded.get_path_name())
    (ROOT/'Saved/Validation/BirdFlockP4/SceneBlueprintManifest.json').write_text(json.dumps(
        {'asset':PATH,'parent':'/Script/Engine.Actor','component':'/Script/DivineBeastsPresentationClient.DivineBeastsBirdFlockSceneComponent','runtime':'Presentation->Definition->VFX','humanAcceptance':'pending'},ensure_ascii=False,indent=2),encoding='utf-8')

if __name__=='__main__':main()
