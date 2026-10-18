# -*- coding: utf-8 -*-
"""SSLM客户端音频真实uasset冷启动回读检查，不改变资产或游戏逻辑。"""
import unreal

ROOT = "/DBASFXPack_Core/Audio"
CASES = [
    ("UI","S_DBA_UI_Click_01","DA_DBA_SFX_UI_Click",False,False),
    ("UI","S_DBA_UI_Confirm_01","DA_DBA_SFX_UI_Confirm",False,False),
    ("UI","S_DBA_UI_Cancel_01","DA_DBA_SFX_UI_Cancel",False,False),
    ("Spells","S_DBA_Spell_Frost_Cast_01","DA_DBA_SFX_Spell_FrostCast",False,True),
    ("Spells","S_DBA_Spell_Frost_Impact_01","DA_DBA_SFX_Spell_FrostImpact",False,True),
    ("Spells","S_DBA_Spell_Fire_Impact_01","DA_DBA_SFX_Spell_FireImpact",False,True),
    ("Music","S_DBA_Music_Login_01","DA_DBA_SFX_Music_Login",True,False),
    ("Music","S_DBA_Music_Village_01","DA_DBA_SFX_Music_Village",True,False),
    ("Ambience","S_DBA_Ambience_Forest_01","DA_DBA_SFX_Ambience_Forest",True,False)
]
policies = [
    ("S_DBA_SpellShort_Attenuation",unreal.SoundAttenuation),
    ("S_DBA_UI_Click_Concurrency",unreal.SoundConcurrency),
    ("S_DBA_Spell_Short_Concurrency",unreal.SoundConcurrency),
    ("S_DBA_Music_Concurrency",unreal.SoundConcurrency),
    ("S_DBA_Ambience_Concurrency",unreal.SoundConcurrency)
]
# 独立PythonScript命令进程的AssetRegistry可能尚未扫描客户端纯内容插件。
# 先明确同步扫描，避免将磁盘真实.uasset误报成缺失。
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
    ["/DBASFXPack_Core"], True)
lib=unreal.EditorAssetLibrary
for folder,wave_name,def_name,expect_loop,has_attenuation in CASES:
    sw=lib.load_asset(f"{ROOT}/{folder}/Waves/{wave_name}")
    definition=lib.load_asset(f"{ROOT}/{folder}/Definitions/{def_name}")
    if not isinstance(sw,unreal.SoundWave):
        raise RuntimeError("冷读波形资源失败："+wave_name)
    if not isinstance(definition,unreal.GamePlatformSFXDefinition):
        raise RuntimeError("冷读SFX定义失败："+def_name)
    if bool(sw.get_editor_property("looping")) != expect_loop:
        raise RuntimeError("SoundWave循环设置错误："+wave_name)
    actual_sound=definition.get_editor_property("sound")
    actual_att=definition.get_editor_property("attenuation")
    actual_con=definition.get_editor_property("concurrency")
    if not actual_sound or not actual_con:
        raise RuntimeError("SFX定义缺真实波形/并发资源："+def_name)
    if has_attenuation and not actual_att:
        raise RuntimeError("三维技能未配置空间衰减："+def_name)
    print("SSLM_AUDIO_COLD_ASSET_OK",def_name,"wave",str(actual_sound),"policy",str(actual_con),"attenuation",str(actual_att))

for name,expected_cls in policies:
    path=f"{ROOT}/Shared/Policies/{name}"
    obj=lib.load_asset(path)
    if not isinstance(obj,expected_cls):
        raise RuntimeError("冷读音频策略资源失败："+name)
    print("SSLM_AUDIO_COLD_POLICY_OK",name)

print("SSLM_AUDIO_COLD_VERIFY_OK",len(CASES),len(policies))
