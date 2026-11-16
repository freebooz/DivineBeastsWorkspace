# -*- coding: utf-8 -*-
"""Monolith 真实资产回归：十二角色第7/8槽须符合当前投射物执行合同；只读。"""
from pathlib import Path
import json
import unreal

HEROES = ('Rat', 'Ox', 'Tiger', 'Rabbit', 'Dragon', 'Snake', 'Horse',
          'Goat', 'Monkey', 'Rooster', 'Dog', 'Boar')
ROOT = '/DBAGameplay/Abilities/Placeholders/HotbarFallbacks'


def main(report_name='MotionAudit.json'):
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    assert project.name == 'DivineBeastsArena.uproject'
    records, errors = [], []
    # 独立批准值，不调用修复器或从作者函数取得期望。
    for hero in HEROES:
        for slot, mode, radius, turns, count, interval in (
                ('Slot6', unreal.DivineBeastsPlaceholderAbilityMode.FROSTBOLT, 0., 0., 1, 0.),
                ('Slot7', unreal.DivineBeastsPlaceholderAbilityMode.ARCANE_MISSILES, 18., 3., 3, .2)):
            path = f'{ROOT}/GA_DBA_{hero}_Hotbar{slot}'
            cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
            assert cls, path
            cdo = unreal.get_default_object(cls)
            assert isinstance(cdo, unreal.DivineBeastsPlaceholderGameplayAbility), path
            motion = cdo.get_editor_property('ProjectileMotion')
            actual = [float(motion.get_editor_property(p)) for p in (
                'SpeedCmPerSecond', 'RangeCm', 'CollisionRadiusCm', 'HelixRadiusCm',
                'HelixTurnsPerSecond', 'InitialPhaseTurns')]
            actual += [int(cdo.get_editor_property('ProjectileCount')),
                       float(cdo.get_editor_property('ProjectileIntervalSeconds'))]
            expected = [3000., 5000., 8., radius, turns, 0., count, interval]
            passed = (cdo.get_editor_property('PlaceholderMode') == mode and
                      all(abs(a-b) < .0001 for a, b in zip(actual, expected)))
            record = {'asset': path, 'motion_count_interval': actual,
                      'expected': expected, 'passed': passed}
            records.append(record)
            if not passed:
                errors.append(record)
    result = {'passed': not errors, 'checked': len(records), 'failed': len(errors),
              'records': records, 'network_verified': False}
    output = project.parents[1] / 'Saved/Validation/AbilityCastingFailure20261025' / report_name
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'passed': not errors, 'checked': len(records), 'failed': len(errors),
                      'report': str(output)}, ensure_ascii=False))
    assert not errors, '技能7/8投射参数不符合服务器执行合同，见 ' + str(output)
    return result


if __name__ == '__main__':
    main()
