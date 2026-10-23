# DBASFXPack_Core（神兽联盟公共音效内容包）

本第三层内容包仅拥有声音与定义，不维护任何C++模块、音频设备或第二套SFX执行器。Client/Editor显式启用，Dedicated Server显式禁用；不得把声音写入Village服务器AlwaysCook目录。

第一批实际资源由现有 Tools/Unreal/Weather/AuthorWeatherAudioAssets.py 从已记录SHA的3份原创合成WAV导入：小雨、大雨和风循环SoundWave，以及雨/雪2份GamePlatformSFXDefinition。既有DefaultGame.ini已有本内容包定义的AssetManager扫描路径，音频文件仍由客户端专属Cook范围收录。

原始声音生成器、来源与哈希保持在 Tools/Unreal/Weather/WeatherAudioSourceManifest.json；这是原创合成参考音，不是最终录音音效，听觉品质、循环无爆音、混音及设备体验需人工确认。P7审核场景可直接使用这些真实SoundWave或派生开发定义，但开发审核地图不进入正式包。

只有真实资源导入和独立UE回读之后才能标记可演示。单元测试、代码编译与文件数量不代替听觉验收和服务器容器排除验证。

## 默认背景音乐（2026-10-23）

用户指定本机视频中的 `Golden Autumn Pond` 作为默认背景音乐。`SourceArt/Audio/Music/GoldenAutumnPond.m4a`保留原AAC音轨，配套WAV为未裁剪、未变速、未重采样的44.1kHz双声道PCM；两者与视频解码后的PCM哈希一致。

真实SoundWave为`/DBASFXPack_Core/Audio/Music/Waves/S_DBA_Music_GoldenAutumnPond`，由Default、Login、Village三个SFX定义共享。沿用平台唯一音乐播放、用户音量、并发、淡入淡出和世界清理链，原有音乐资产保留。来源说明与验证状态见`Docs/GoldenAutumnPondImport_20261023.json`；本曲目来源是用户提供的视频，不沿用旧素材的CC0标记。
