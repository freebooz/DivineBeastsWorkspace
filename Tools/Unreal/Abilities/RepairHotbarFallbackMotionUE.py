# -*- coding: utf-8 -*-
"""通过 Monolith 执行：仅修正旧副本弹速，备份、编译、保存并核对其它属性未变化。"""
from pathlib import Path
import hashlib
import json
import shutil
import unreal


def snapshot(cdo):
    """保护本次修复相关的身份、运动、成本、冷却和服务器执行策略。"""
    result = {p: str(cdo.get_editor_property(p)) for p in (
        'AbilityDefinitionId', 'PlaceholderMode', 'ProjectileCount',
        'ProjectileIntervalSeconds', 'CostGameplayEffectClass',
        'CooldownGameplayEffectClass', 'PlaceholderCooldownTags', 'NetExecutionPolicy')}
    motion = cdo.get_editor_property('ProjectileMotion')
    result['motion'] = {p: float(motion.get_editor_property(p)) for p in (
        'SpeedCmPerSecond', 'RangeCm', 'CollisionRadiusCm', 'HelixRadiusCm',
        'HelixTurnsPerSecond', 'InitialPhaseTurns')}
    # 结构体的 str 包含瞬时地址，稳定身份使用原生 export_text。
    for p in ('AbilityDefinitionId', 'PlaceholderCooldownTags'):
        result[p] = cdo.get_editor_property(p).export_text()
    return result


def main():
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    root = project.parents[1]
    assert project.name == 'DivineBeastsArena.uproject'
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '先保留未保存资产'
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '先保留未保存地图'
    out = root / 'Saved/Validation/AbilityCastingFailure20261025'
    red = json.loads((out/'RED.before.json').read_text(encoding='utf-8'))
    assert red['checked'] == 24 and red['failed'] == 24
    plans = []
    for entry in red['records']:
        path = entry['asset']
        bp = unreal.load_asset(path)
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
        motion = cdo.get_editor_property('ProjectileMotion')
        old_speed = float(motion.get_editor_property('SpeedCmPerSecond'))
        assert old_speed in (1400., 1600.), (path, old_speed)
        before = snapshot(cdo)
        disk = root/'Game/Plugins/DivineBeasts/DBAGameplay/Content'/(
            path.removeprefix('/DBAGameplay/')+'.uasset')
        backup = out/'Backup'/disk.relative_to(root)
        assert disk.is_file() and not backup.exists(), str(disk)
        plans.append((path, bp, cdo, motion, before, disk, backup, old_speed))
    # 所有路径及前置状态确认后才逐项提交，不覆盖其它会话的未保存编辑。
    for path, bp, cdo, motion, before, disk, backup, old_speed in plans:
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(disk, backup)
    report = []
    for path, bp, cdo, motion, before, disk, backup, old_speed in plans:
        motion.set_editor_property('SpeedCmPerSecond', 4000.)
        cdo.set_editor_property('ProjectileMotion', motion)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False), path
        actual = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
        before['motion']['SpeedCmPerSecond'] = 4000.
        assert before == snapshot(actual), path
        report.append({'asset': path, 'old_speed': old_speed, 'speed': 4000.,
                       'backup': str(backup), 'sha256': hashlib.sha256(disk.read_bytes()).hexdigest()})
    (out/'NativeRepair.saved.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('SSLM_HOTBAR_MOTION_REPAIRED=' + str(len(report)))
    return report


if __name__ == '__main__':
    main()
