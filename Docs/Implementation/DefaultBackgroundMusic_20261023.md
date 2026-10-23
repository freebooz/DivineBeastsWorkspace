# SSLM默认背景音乐导入（2026-10-23）

用户指定本机视频`Golden Autumn Pond  Beautiful Relaxing Music, Peaceful Soothing Instrumental Music by Tim Janis.mp4`中的音频作为默认背景音乐。本次保留完整517.317秒、44.1kHz、双声道内容；不裁剪、变速或重新采样。

## 资源与接入

- 原AAC音轨：`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/SourceArt/Audio/Music/GoldenAutumnPond.m4a`；原采样率16位PCM为同目录WAV。视频、M4A、WAV解码后的PCM SHA256均为`b1bcb108f0cc41cbda9c6db9de5b608fe23f5f2115a284bbd965afbb686d074c`。
- 真实SoundWave：`/DBASFXPack_Core/Audio/Music/Waves/S_DBA_Music_GoldenAutumnPond`，通过Monolith MCP和UE AssetTools导入，循环、PrimeOnLoad、PlayWhenSilent、压缩质量80、原始音量/音高1.0。
- 既有Login/Village音乐定义共享这份曲目，保留稳定身份、原有淡入淡出和并发策略。新增`DA_DBA_SFX_Music_Default`及`presentation.dba.audio.music.default@1`供其他游戏世界回退使用。
- 播放继续使用项目语义目录→GamePlatformPresentation→GamePlatformSFX→GamePlatformData的唯一链路。没有新增播放器、轮询或用户音量真源。
- 项目首次音乐音量沿用现有`Music=0.0`；用户保存值优先。验证期间仅把隔离PIE世界音乐音量临时设为30%，结束时恢复。音量偏好未获新答复，因此本次没有把既有默认静音改成自动开启。

本曲目由用户提供，来源与哈希独立记录，不沿用既有其它素材的CC0标记。旧音乐文件保留，客户端容器门禁已改为核对实际使用的新曲目及三份定义。

## 已完成验证

| 项目 | 结果及证据 |
| --- | --- |
| 完整音频一致性 | `Saved/Validation/DefaultMusic20261023/Extraction.json`，三份完整解码PCM相同 |
| 独立UE冷加载 | `ColdValidation.log`、`ColdReloadFinal.json`，独立进程退出0，确认时长、双声道、循环、流式、静音恢复、三个唯一逻辑身份和共享引用 |
| 完整Editor增量构建 | `Saved/Validation/FoundationM0/88df9f28-9552-4c25-a7c4-a71c629a40bc/Build/result.json`，Passed/0 |
| 完整Client增量构建 | `Saved/Validation/FoundationM0/274ac70f-26fa-4af9-a7ac-a07363bc2e8e/Build/result.json`，Passed/0 |
| 客户端Cook/Stage | `Saved/Validation/FoundationM0/316f384e-a01d-4c0b-ae65-7e290360c069/Cook-FrontEndClient/result.json`，Passed/0，629项必需资源、82项音频路径，缺失0；实际容器包含新曲目导出/流式Bulk和三个定义 |
| 登录音乐实际混音 | `Playback_FrontEnd_ActiveWindow.json`及对应WAV/分析报告；一个播放实例，末5秒RMS 0.009151，峰值0.048798 |
| 竞技场默认回退 | `Playback_MainArena_Default.json`及对应WAV/分析报告；正式Default语义被受理，一个播放实例，末5秒RMS 0.003266，峰值0.027161 |
| 世界清理 | 停止PIE后本曲目仍在播放的组件数量0；临时音乐音量及编辑器SoloAudio设置已恢复 |

所有PIE播放取证在独立离线世界完成，不等于Village/MainArena联机准入或用户听感签收。初次后台PIE录音为静音；把本次测试编辑器置于前台后取得真实非零主混音信号，因此没有把`Submitted`或`IsPlaying`单独当成实际出声证据。

本次构建还对共享依赖中三处不兼容UE5.8 `TObjectPtr`的自动指针推导做了最小类型修正，未改积雪行为：`GamePlatformSurfaceSnowContacts.cpp`中的Pawn声明及`DivineBeastsVillageSnowWorldSubsystem.cpp`中的两个范围循环变量。没有改动共享战斗合同或账号/角色数据。

实际客户端程序位于上述Cook目录下`Stage/WindowsClient/DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe`。本次未启动打包客户端或切换实际游戏服务器；后续联机验证继续执行客户端与Village/MainArena战斗版本一致性门禁。
