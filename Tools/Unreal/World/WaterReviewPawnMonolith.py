"""为既有Water审核图绑定数据型GameMode/Pawn，直接复用主线泳潜输入和动画。"""
import json
import importlib.util
import os
from pathlib import Path
import unreal as u


def author():
    script = Path(u.Paths.project_dir()).resolve().parent/'Tools/Unreal/World/VillageWaterSystemsMonolith.py'
    spec = importlib.util.spec_from_file_location('water_common', script)
    wa = importlib.util.module_from_spec(spec); spec.loader.exec_module(wa)
    world, _ = wa.world_actors()
    assert world.get_path_name().split('.')[0] == wa.REVIEW
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '目标图已脏，禁止覆盖'
    assets = u.EditorAssetLibrary
    root = '/Game/Development/Water/Blueprints'
    created = []
    def blueprint(name, parent):
        path = root+'/'+name
        assert not assets.does_asset_exist(path), '不覆盖已有Blueprint: '+path
        factory = u.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        factory.set_editor_property('edit_after_new', False)
        bp = u.AssetToolsHelpers.get_asset_tools().create_asset(name, root, u.Blueprint, factory)
        assert bp
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        created.append(path)
        return bp
    pawn = blueprint('BP_DBA_WaterReviewCharacter', u.load_class(None, '/Script/DivineBeastsAbilitiesRuntime.DivineBeastsGameplayCharacter'))
    defaults = u.get_default_object(pawn.generated_class())
    defaults.mesh.set_skeletal_mesh_asset(u.load_asset('/DBAContentPack_Common/Characters/KachujinGRosales/Kachujin_G_Rosales'))
    defaults.mesh.set_editor_property('relative_location', u.Vector(0, 0, -90))
    defaults.mesh.set_editor_property('relative_rotation', u.Rotator(pitch=0, yaw=-90, roll=0))
    defaults.mesh.set_anim_instance_class(u.load_class(None, '/DBAContentPack_Common/Characters/KachujinGRosales/Animations/ABP_DBA_Kachujin.ABP_DBA_Kachujin_C'))
    assert assets.save_loaded_asset(pawn, False)
    mode = blueprint('BP_DBA_WaterReviewGameMode', u.GameModeBase)
    u.get_default_object(mode.generated_class()).set_editor_property('default_pawn_class', pawn.generated_class())
    assert assets.save_loaded_asset(mode, False)
    wa.backup(wa.REVIEW)
    world.get_world_settings().set_editor_property('default_game_mode', mode.generated_class())
    assert u.EditorLoadingAndSavingUtils.save_map(world, wa.REVIEW)
    report = {'pid': os.getpid(), 'map': wa.REVIEW, 'assets': created,
              'input': 'WASD, view-pitch forward, Space ascend, LeftCtrl descend; no oxygen/drowning',
              'runtimeMechanism': 'existing DivineBeastsGameplayCharacter and native CharacterMovement'}
    (wa.OUT/'ReviewPawnAuthored.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('WATER_REVIEW_PAWN_SAVED', report)
