# -*- coding: utf-8 -*-
"""在自有审核地图中验证未保存地图保护；必须拒绝捕获且保持World/Actor不变。"""
from pathlib import Path
import json,runpy
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash/Review/L_DBA_Review_AttackSlashV2.')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
marker=actors.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(0,0,-200));marker.set_actor_label('TEMP_SlashUnsavedMapGuard')
assert unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
try:
    try:runpy.run_path(str(ROOT/'Tools/Unreal/VFX/CommonSkillVFX/CaptureAttackSlashV2UE.py'))
    except RuntimeError as e:assert 'UNSAVED_PACKAGES:' in str(e)
    else:raise AssertionError('未保存地图未被拒绝')
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()==world and unreal.SystemLibrary.is_valid(marker)
    report=dict(passed=True,unsavedMapPreserved=True,world=world.get_path_name())
    (ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2/CaptureGuard.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print('ATTACK_SLASH_V2_UNSAVED_MAP_GUARD_PASSED')
finally:
    if unreal.SystemLibrary.is_valid(marker):actors.destroy_actor(marker)
