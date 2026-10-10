# 天气真实引擎资产制作和内容归属

当前天气源码是可复用运行时机制，**没有通过文本伪造任何.uasset、.umap或Niagara System**。所有资源必须由锁定UE5.8实际创建、编译、保存、关闭重载回读。

## 资源归属

平台默认雨、雪、雷、电、雾的通用Niagara定义与实例归`GamePlatformVFX`实际内容及`GamePlatformPresentation`语义目录；湿润、积雪、积水母材质/材质函数归`GamePlatformSurface`。项目专属风格化效果、音频、地形材质实例和天气Preset资产归对应正式登记的`DBAWorldPack_*`或项目公共表现内容包；不得新建未经登记的空内容插件。

## 第一轮资产制作清单

1. 在`GamePlatformSurface`中通过正式`GamePlatformSurfaceCoreAssets` Commandlet生成和验证`/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`，使用材质编辑器真实创建`M_GP_Surface_Master`、`M_GP_Surface_Lite`、`MF_GP_SnowLayer`、`MF_GP_WetnessLayer`和`MF_GP_PuddleLayer`，检查Shader编译及Material Stats。
2. 为雨/雪制作至少`NS_GP_Weather_Rain`、`NS_GP_Weather_Snow`的Niagara System（名称为建议，不代表已有资产）；雨滴随相机小范围局部生成，按照高/低质量档位裁剪碰撞与密度。粒子结束/切图需释放实例。
3. 在GamePlatformPresentation目录分别登记已真实存在的`Presentation.Weather.Rain.VFX`、`Presentation.Weather.Snow.VFX`与SFX类型的映射，定义缺失时不能用虚假DefinitionId代替。
4. 将雨声/风声/雷声通过现有SFX真实播放器/Provider加载，并按不同天气强度进行混合，不在Weather插件新造音频播放器。
5. 新手村的山体、屋瓦、土地`MI_DBA_*`接入Surface MPC；地图蓝图/项目GameMode可以编辑`InitialWeather`和自动调度表，避免在客户端关卡蓝图写权威天气。
6. 通过人工场景观察：雨停后雨滴停止、地面逐渐干燥；雪停后飘雪停止而积雪缓慢变化；室内避雨、摄像机移动和多材质交界不出现穿帮。

## P5验收证据

逐资源记录包路径、资产原生类型、编译日志、保存/回读状态、相关Preview截图、ProfileGPU测量、Client Cook/Stage验证和Dedicated Server包资产剥离。没有这些证据不得将P5标记完成。

## 2026-10-10 真实SourceArt交付（不等于UE资源交付）

- 已生成9张PNG源纹理：`GamePlatformVFX/SourceArt/Weather`四张雨雪粒子透明贴图，`GamePlatformSurface/SourceArt/Weather`五张雪／湿润／积水PBR技术源纹理；含4×4水花Flipbook及4×4雪花Atlas。
- 已生成原创合成雨／风WAV音源3段（48kHz、16-bit、双声道、12秒），暂存`DBAWorldPack_Village/SourceArt/Weather/Audio`；**严禁将实际SoundWave放入Server AlwaysCook的新手村共享Content目录**。以后真正的音频资产应交付项目客户端独立内容包`DBASFXPack_Core`，按真实资产完成后再建立插件与登记；通用音效播放机制仍属于`GamePlatformSFX`。
- 源纹理生成／校验：`Tools/Unreal/Weather/GenerateWeatherSourceTextures.py`、`WeatherSourceArtManifest.json`；声音生成／校验：`GenerateWeatherAudio.py`、`WeatherAudioSourceManifest.json`。合成声音已修复首尾循环接缝，均已按SHA256检查。
- `Tests/Architecture/ValidateWeatherSourceArt.py`检查图像尺寸、透明度、无缝边界、雪法线方向及WAV声学基本参数。 `Tools/Unreal/Weather/ImportWeatherSourceArt.py`仅在正式UE PythonScriptPlugin中且设置`WEATHER_ASSET_IMPORT_MODE=apply`后导入Texture2D；普通Python运行仅做只读预检。
- 正式工程上实际尝试`GamePlatformSurfaceCoreAssets` Commandlet返回1，`GamePlatformServer`模块缺DLL。引擎资产、MPC、Niagara、材质母版与声音Definition均未交付。完整证据：`Docs/Implementation/WeatherSourceAssetsExecution_20261010.md`。
