# -*- coding: utf-8 -*-
"""锁定UE编辑器中的世界移动资源回归，不修改地图、角色或预览Idle。

真实加载世界ABP、三段移动序列和BlendSpace，拒绝旧UE4骨架、缺失资源、
纯根运动移动及遗留模板依赖；失败抛异常，不把静态文件存在视为运行验收。
"""
import json
from pathlib import Path
import unreal

BASE = '/DBAContentPack_Common/Mannequins/DBA/Animations/'
SKELETON = '/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin'


def require_asset(path):
    """加载已保存引擎资源；资源缺失属于交付失败，不创建测试替身。"""
    asset = unreal.load_asset(path)
    if not asset:
        raise AssertionError('缺少世界移动资源: ' + path)
    return asset


def validate():
    """检查实际资源合同并输出可留存证据；不冒充客户端移动或联网验收。"""
    skeleton = require_asset(SKELETON)
    blueprint = require_asset(BASE + 'ABP_DBA_WorldLocomotion')
    assert blueprint.get_editor_property('target_skeleton') == skeleton, '世界ABP必须使用完整UE5骨架'
    parent = blueprint.get_editor_property('parent_class')
    assert parent.get_path_name() == '/Script/GamePlatformAnimationClient.GamePlatformLocomotionAnimInstance', '世界动画必须读取Pawn真实移动状态'
    assert unreal.EditorAssetLibrary.load_blueprint_class(BASE + 'ABP_DBA_WorldLocomotion'), '世界ABP必须编译出真实类'
    blend = require_asset(BASE + 'BS_DBA_WorldLocomotion')
    assert blend.get_editor_property('skeleton') == skeleton, '移动BlendSpace骨架一致'
    samples = list(blend.get_editor_property('sample_data'))
    assert len(samples) == 3, 'Idle/Walk/Run必须都是实际采样点'
    speeds = sorted(sample.sample_value.x for sample in samples)
    assert speeds == [0.0, 150.0, 500.0], '速度轴以厘米/秒定义'
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    records = []
    for name in ('MM_Idle', 'MF_Unarmed_Walk_Fwd', 'MF_Unarmed_Jog_Fwd', 'MM_Fall_Loop'):
        sequence = require_asset(BASE + 'WorldLocomotion/' + name)
        assert sequence.get_skeleton() == skeleton, name + '骨架一致'
        assert sequence.get_play_length() > 0.25, name + '必须有真实动画长度'
        assert not sequence.get_editor_property('enable_root_motion'), name + '不能替代CharacterMovement权威位移'
        if name in ('MF_Unarmed_Walk_Fwd', 'MF_Unarmed_Jog_Fwd'):
            # 读取引擎实际压缩姿势，避免模板FK骨树未初始化时“有长度、会加载、仍是静止参考姿势”。
            options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED)
            rotations = []
            for time in (0.0, 0.375):
                pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
                rotations.append(unreal.AnimPoseExtensions.get_bone_pose(pose, 'thigh_l').rotation)
            first, second = rotations
            difference = min(sum((getattr(first, axis) - sign * getattr(second, axis)) ** 2 for axis in ('x', 'y', 'z', 'w')) for sign in (-1, 1))
            assert difference > 0.001, name + '腿骨必须随时间真实运动'
        dependencies = [str(value) for value in registry.get_dependencies(sequence.get_outermost().get_name(), unreal.AssetRegistryDependencyOptions())]
        assert not any(value.startswith('/Game/Characters/') for value in dependencies), name + '不能留下模板挂载依赖'
        records.append({'asset': sequence.get_path_name(), 'durationSeconds': sequence.get_play_length(), 'dependencies': dependencies})
    preview = require_asset(BASE + 'ABP_DBA_PreviewIdle')
    assert preview != blueprint, '角色选择预览必须保留独立Idle'
    report = {'project': unreal.Paths.get_project_file_path(), 'skeleton': skeleton.get_path_name(), 'parent': parent.get_path_name(), 'sampleSpeedsCmPerSecond': speeds, 'sequences': records}
    destination = Path(unreal.Paths.project_saved_dir()).parent.parent / 'Saved/Validation/WorldLocomotion/20261010/Assets.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


validate()
