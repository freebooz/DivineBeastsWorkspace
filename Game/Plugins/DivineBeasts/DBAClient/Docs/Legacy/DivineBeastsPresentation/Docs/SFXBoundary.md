# SFXBoundary（声音边界）

SFX执行归GamePlatformSFX，项目Presentation不创建Audio Component、不控制Mixer、不直接播放SoundWave/SoundCue。

未来Project/ContentPack Catalog可使用ProviderChannel=SFX与逻辑DefinitionId选择声音语义，实际资源和预加载由SFX Provider负责。

DBASFXPack_Core当前不存在真实资产/插件，所以声音内容包集成为“未执行”。
