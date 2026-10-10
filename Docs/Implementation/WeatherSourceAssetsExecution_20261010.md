# 《神兽联盟》天气真实资源P5实施证据（2026-10-10）

项目：`DivineBeastsWorkspace`，主工程`Game/DivineBeastsArena.uproject`，引擎`F:\UnrealEngine-5.8.0-release`。
范围：天气SourceArt技术美术纹理、原创天气基础音频、只读与将来UE导入流程；无新代码插件、无服务器玩法更改。

## 已完成的真实工作

1. 新建`Tools/Unreal/Weather/GenerateWeatherSourceTextures.py`，真实生成并复核9张PNG原始纹理：
   - VFX：RainStreak、RainRipple、RainSplashFlipbook（4×4）、SnowflakeAtlas（4×4）四张。
   - Surface：WetnessNoise、PuddleMask、SnowAlbedo、SnowNormal、SnowORM五张。
   - 图像尺寸512×512／512×1024／1024×1024，RGBA/RGB/L通道真实存在，各独立文件SHA256已写入`Tools/Unreal/Weather/WeatherSourceArtManifest.json`。
2. 新建`Tools/Unreal/Weather/GenerateWeatherAudio.py`，合成可播放的WAV：RainLight、RainHeavy、Wind各一段；48kHz、16bit、立体声、12秒，无第三方采样。第一次质量门禁发现循环首尾接缝不连续；现将最后10ms与开头反向窗平滑对齐、受SHA256保护完成修复并重新生成，指纹位于`Tools/Unreal/Weather/WeatherAudioSourceManifest.json`。
3. 新建`Tests/Architecture/ValidateWeatherSourceArt.py`：检查9张PNG实际尺寸与模式、完整性、透明度占比、16格雪花非空、Surface纹理边缘连续与雪法线方向；检查3份WAV声道/采样率/位宽/时长/均方根/峰值/循环边界。
4. 新建`Tools/Unreal/Weather/ImportWeatherSourceArt.py`：运行在普通Python时只读检验9份资源SHA与预期UE目标，明确`PREVIEW_ONLY`；必须通过实际UE编辑器、`WEATHER_ASSET_IMPORT_MODE=apply`显式导入Texture2D，且任何已存在目标拒绝覆盖。
5. 没有写入或伪造任何`.uasset/.umap`。雨雪Niagara、Material/MaterialFunction、真实MPC、SoundWave、MetaSound、VFX/SFX Definition和Catalog，均**未**声称已创建。

## 阻断与风险

- 真实运行`UnrealEditor-Cmd`执行`GamePlatformSurfaceCoreAssets`返回1，LogPluginManager报告`GamePlatformServer`模块未找到；因而无法实际创建`MPC_GP_SurfaceGlobal`。
- 编辑器完整构建缺少多个DLL。只有单文件C++编译成功不能证明GamePlatformWeather/Surface动态库可加载；现阶段不能通过Monolith成功完成真实Niagara材质资产。
- 本轮还尝试`Build.bat DivineBeastsArenaEditor Win64 Development -Module=GamePlatformWeatherRuntime`定向链接；UBT在当前工程中列出138个动作，但执行入口被工作区另一个`Build.bat`运行实例互斥占用，等待220秒后超时退出，**没有生成Weather模块DLL，不计构建通过**。不强杀其他会话构建、不通过禁用必要插件伪造正常编辑器加载。
- `VillageServer/DefaultGame.ini`对整个`/DBAWorldPack_Village` AlwaysCook。小雨/大雨/风声只存`SourceArt`，**正式SoundWave不得直接导入Village共享Content目录**；须等有实资产时登记客户端独立`DBASFXPack_Core`或经过服务器Cook剥离证明的合规拥有者。
- 源美术是可使用的技术底稿，不能等同人工打磨的AAA贴图或专业录音；需进一步制作PBR完整材质、Niagara与试听混音。

## 继续实施的门槛## 2026-10-10 先代码与蓝图、再集中测试的新批次

用户已明确将工作顺序调整为“完成代码/蓝图与制作方案，最后统一自动化测试”。本阶段新增：
- 平台层`UGamePlatformWeatherBlueprintLibrary`真正蓝图API，项目层`ADivineBeastsWeatherReviewController`功能Actor（可派生蓝图、不发权威RPC）；
- `Tools/Unreal/Weather/AuthorWeatherSurfaceAssets.py`（7个函数＋2个真实材质图）、`AuthorWeatherVFXMaterials.py`（3个粒子材质）、`AuthorWeatherBlueprintAssets.py`（8份天气定义＋1个实际审核蓝图）、`AuthorWeatherVFXDefinitions.py`（真实Rain/Snow Niagara存在才制作DataAsset）、`AuthorWeatherAudioAssets.py`（客户端独立声音包存在才导入SoundWave并制作定义）和`WeatherNiagaraAuthoringSpec_V1.json`（真实Niagara编辑器模块/发射器/渲染器制作合同）。
- `GamePlatformPresentationCore`增加中立浮点参数、响度系数和弱附着目标；VFX/SFX桥透传到各自请求，天气客户端为雨雪设置可验证的`User.WeatherIntensity`或`WeatherIntensity`，持续天气附着本地ViewTarget组件而不是世界原点；`DBAClient`新增雨、雪VFX和SFX目录构造器（**未激活**，避免缺资源造成伪成功）。
- Editor模块构建因引擎级编译89个动作且超时，**未**完成Weather/Server/SFX动态库链接，也未创建目标MPC、Texture2D、Material、Blueprint、Niagara、MetaSound资产。其他会话PCG/UI未提交文件已保留。此项属于代码完成阶段，不应填作UE资源交付。

最后统一验证范围：Python脚本语法和源素材、三层架构门禁、C++定向编译、真实UE Editor启动与生成脚本、Shader/蓝图/Niagara编译回读、双客户端与Cook/性能；实际未执行项目单独标为待验证。

## 继续实施的门槛

1. Editor完整模块链接并使用正式`DivineBeastsArena.uproject`成功进入编辑器。
2. 真正执行MPC创建＋ValidateOnly回读，随后通过UE Python或Monolith导入PNG生成Texture2D；确保资源软引用和Package路径正确。
3. Surface MateriaI母材质、MF函数真实制作并Shader编译；Niagara雨、雪、飞溅效果及其VFX Definition与Presentation Catalog真实创建/装配。
4. 在客户端独立音效内容拥有者登记完成后导入WAV为SoundWave，建立SFX Definition和MetaSound混音，使用GamePlatformSFX播放。
5. Village实例下单客户端及双客户端晴雨雪切换、停止和重连实测；性能、Cook与Server资产剥离单独留证。

## 验收责任边界

源码和源素材验证，不代替UE资产存在、材质Shader成功、Niagara播放成功或网络同步成功。本次未执行提交或推送，也不得将并行UI/PCG工作区改动归入天气交付。
