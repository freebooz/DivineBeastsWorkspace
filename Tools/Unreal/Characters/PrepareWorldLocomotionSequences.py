# -*- coding: utf-8 -*-
"""由Monolith在锁定UE编辑器内修正已复制的引擎模板真实移动序列。

输入为UE5.8 Templates/TemplateResources/High/Characters中的原生uasset副本，
四个序列保持模板名称，归已登记公共开发角色包。先用Monolith显式绑定完整UE5骨架；
本脚本关闭根运动、锁定根骨并修正软引用，仅保存这四个资产，不修改世界或PreviewIdle。
失败抛异常，重跑不覆盖用户资源；回退仅移除本轮新增四资源和世界ABP/BlendSpace。
"""
import json
import unreal

BASE = '/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/'
SKELETON = '/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin'
skeleton = unreal.load_asset(SKELETON)
if not skeleton:
    raise RuntimeError('缺少完整UE5公共骨架')
redirects = unreal.Map(unreal.SoftObjectPath, unreal.SoftObjectPath)
redirects[unreal.SoftObjectPath('/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin')] = unreal.SoftObjectPath(SKELETON + '.SK_Mannequin')
for name in ('SKM_Manny_Simple', 'SKM_Quinn_Simple'):
    redirects[unreal.SoftObjectPath('/Game/Characters/Mannequins/Meshes/' + name + '.' + name)] = unreal.SoftObjectPath('/DBAContentPack_Common/Mannequins/DBA/Meshes/' + name + '.' + name)
for relative in ('MM_Idle', 'Walk/MF_Unarmed_Walk_Fwd', 'Jog/MF_Unarmed_Jog_Fwd', 'Jump/MM_Fall_Loop'):
    name = relative.rsplit('/', 1)[-1]
    # 模板部分序列保存自身原始软路径；迁移后同步到本包，不能留下Cook时不可解析的旧/Game依赖。
    redirects[unreal.SoftObjectPath('/Game/Characters/Mannequins/Anims/Unarmed/' + relative + '.' + name)] = unreal.SoftObjectPath(BASE + name + '.' + name)
saved = []
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for name in ('MM_Idle', 'MF_Unarmed_Walk_Fwd', 'MF_Unarmed_Jog_Fwd', 'MM_Fall_Loop'):
    sequence = unreal.load_asset(BASE + name)
    if not sequence or sequence.get_skeleton() != skeleton:
        raise RuntimeError('必须先通过Monolith绑定完整UE5骨架: ' + name)
    # 权威位移由CharacterMovement结算；模板根位移只作姿势来源，强制锁根避免网格越过胶囊。
    sequence.set_editor_property('enable_root_motion', False)
    sequence.set_editor_property('force_root_lock', True)
    sequence.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.REF_POSE)
    unreal.AssetToolsHelpers.get_asset_tools().rename_referencing_soft_object_paths([sequence.get_outermost()], redirects)
    # 直接复制模板后旧挂载Skeleton暂时为空，Sequencer FK Rig尚未初始化；只改Skeleton字段仍会输出参考姿势。
    # 原生控制器恢复当前UE5骨树与既有模板轨道，再重新压缩，不能把成功加载/长度字段当成实际动态姿势。
    unreal.SystemLibrary.execute_console_command(editor_world, 'GP.Animation.InitializeDataModel ' + sequence.get_path_name())
    if not unreal.EditorAssetLibrary.save_loaded_asset(sequence, only_if_is_dirty=False):
        raise RuntimeError('世界移动序列保存失败: ' + name)
    saved.append({'asset': sequence.get_path_name(), 'durationSeconds': sequence.get_play_length()})
print(json.dumps({'saved': saved}, ensure_ascii=False))
