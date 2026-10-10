# -*- coding: utf-8 -*-
"""在锁定UE编辑器内验证角色姿势，拒绝有长度但实际输出参考姿势的动画。

项目公共表现资源只读门禁：调用方为Monolith；不修改模型、地图或权威移动。
压缩姿势是客户端实际消费的结果，不能以轨道数量或资源存在代替动画验收。
"""
import unreal

BASE = '/DBAContentPack_Common/Mannequins/DBA/Animations/'
OPTIONS = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.COMPRESSED)


def quaternion_distance(first, second):
    """四元数正负表示相同旋转；返回两种符号中的最小平方差。"""
    return min(sum((getattr(first, axis) - sign * getattr(second, axis)) ** 2
                   for axis in ('x', 'y', 'z', 'w')) for sign in (-1, 1))


def validate_non_reference_pose(sequence):
    """在动画中段读取压缩姿势；任一主要肢体必须偏离绑定姿势。"""
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, sequence.get_play_length() * 0.5, OPTIONS)
    differences = []
    for bone in ('thigh_l', 'calf_l', 'upperarm_l', 'lowerarm_l', 'spine_03'):
        actual = unreal.AnimPoseExtensions.get_bone_pose(pose, bone)
        reference = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone)
        differences.append(quaternion_distance(actual.rotation, reference.rotation))
    assert max(differences) > 0.001, sequence.get_name() + '实际压缩结果仍为参考姿势'


fall = unreal.load_asset(BASE + 'WorldLocomotion/MM_Fall_Loop')
assert fall, '下落资源必须存在'
validate_non_reference_pose(fall)
for phase in ('MM_Jump', 'MM_Land'):
    animation = unreal.load_asset(BASE + 'WorldLocomotion/' + phase)
    assert animation, phase + '资源必须存在'
    validate_non_reference_pose(animation)
idle = unreal.load_asset(BASE + 'WorldLocomotion/MM_Idle')
assert idle, '待机资源必须存在'
validate_non_reference_pose(idle)
first = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, OPTIONS)
second = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, idle.get_play_length() * 0.4, OPTIONS)
assert any(quaternion_distance(unreal.AnimPoseExtensions.get_bone_pose(first, bone).rotation,
                               unreal.AnimPoseExtensions.get_bone_pose(second, bone).rotation) > 0.000001
           for bone in ('thigh_l', 'upperarm_l', 'spine_03')), '待机必须有真实时间变化'
print('CHARACTER_MOTION_POSES_PASS')
