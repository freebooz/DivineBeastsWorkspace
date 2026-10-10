# 天气美术源素材及UE导入工具

工作空间：`DivineBeastsWorkspace（神兽联盟工作空间）`；适用虚幻引擎：锁定UE5.8。
这些源文件不产生新的游戏机制插件，也不替代实际材质、Niagara、MetaSound或SoundWave资产。

## 现已产生的源素材（2026-10-10）

- `GamePlatformVFX/SourceArt/Weather/`：雨线、雨水波纹、16帧水花、16格雪花RGBA源纹理4张；
- `GamePlatformSurface/SourceArt/Weather/`：湿润噪声、积水遮罩、雪色、雪微法线、雪ORM共5张；
- `DBAWorldPack_Village/SourceArt/Weather/Audio/`：小雨／大雨／风声源音频3段，每段PCM 48kHz/16-bit/双声道/12秒。这里只存不会被UE Cook的`SourceArt`，**禁止把SoundWave资源放入Shared Village Content**，因为`Game/Config/Custom/VillageServer/DefaultGame.ini`配置整个`/DBAWorldPack_Village`为服务器AlwaysCook。正式通用游戏声音由`GamePlatformSFX`机制播放，实际客户端声音资源应由独立纯内容包`DBASFXPack_Core`持有，在确有真实.uasset时创建/登记该插件。该插件当前仅为规划，不创建空插件或冒充挂载点。

所有源文件均为本工作区原创程序化生成，不含下载或第三方授权素材。雪色、雪微法线、ORM作为**技术美术基础稿**，后续可以在相同源身份内经人工审核替换。模拟天气音频可用作开发基底，但不冒充专业外录成品。

## 生成、检查与安全

在工作区根目录运行：

```powershell
python Tools/Unreal/Weather/GenerateWeatherSourceTextures.py --verify  # 9张PNG SHA256核验
python Tools/Unreal/Weather/GenerateWeatherAudio.py --verify            # 3段WAV SHA256核验
python Tests/Architecture/ValidateWeatherSourceArt.py                   # 源素材形态/接缝/音频参数质量门禁
python Tools/Unreal/Weather/ImportWeatherSourceArt.py                   # 只读打印计划中的UE Texture目标路径
```

真正初次生成时移除两个生成脚本的`--verify`。默认对已有不同文件拒绝覆盖；合成声音若需迭代，仅可使用`--refresh-generated`，且必须校验旧Manifest与每一原WAV的SHA256后才会写入新内容。生成脚本和参数改变后须重新更新指纹和审核源素材；禁止覆盖人工修改的PNG/WAV。

## UE真实纹理导入（当前未执行）

先完成正式`DivineBeastsArenaEditor`三层模块构建、启动正确UE5.8工程并启用Epic`PythonScriptPlugin`（编辑器Python脚本插件）。仅当真实编辑器与工程对应后，显式授权导入：

```powershell
$env:WEATHER_ASSET_IMPORT_MODE = "apply"
& "F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "E:\poject\feebooz\DivineBeastsWorkspace\Game\DivineBeastsArena.uproject" `
  "-ExecutePythonScript=E:/poject/feebooz/DivineBeastsWorkspace/Tools/Unreal/Weather/ImportWeatherSourceArt.py" `
  -unattended -nop4
```

这是待验证的**执行指令**，不是成功记录。它只允许9张纹理进入已有且`CanContainContent=true`的插件：`/GamePlatformVFX/Weather/Textures/`及`/GamePlatformSurface/Textures/Weather/`。脚本先核对SHA、插件挂载与目标重名，默认inspect不写资产；需要`apply`才导入，已存在资源报错不覆盖。导入后按Manifest设置sRGB、Compression并保存/回读Texture2D。

`MPC_GP_SurfaceGlobal`应由已经实现的`GamePlatformSurfaceCoreAssets`真实Commandlet创建与`-ValidateOnly`回读；材质母版和7个Material Function要在Material Editor中建图并编译，不得用同名空.uasset充数。雨雪Niagara应通过编辑器/Monolith构建并真实保存和回读，贴图只提供源输入。

## 现在的引擎阻断（已真实复现）

2026-10-10在正式工程上运行`UnrealEditor-Cmd.exe -run=GamePlatformSurfaceCoreAssets`时，引擎进入模块加载后报告缺失`GamePlatformServer`模块并退出1；未创建MPC。仓库检查多个Editor目标插件DLL也不存在，意味着单文件编译通过≠完整编辑器DLL加载成功。

应先修复正式Editor全目标构建，不建议通过写假二进制、任意禁用必要插件或构造第二个虚假工作区来绕过阻断。UE尚不能加载时只完成SourceArt与预检，不宣称完成P5真实引擎资产、声音或视觉表现。

## 交付和验收

- SourceArt文件可验证、尺寸与通道契约正确、RGBA图具有有效Alpha、雪表面纹理边界无明显接缝。
- WAV文件必须满足PCM规格、无削波、首尾接缝不过度跳变、源SHA未被篡改；仍要求耳机/设备试听与专业美术审核。
- 引擎纹理和MPC须分别保存、编辑器重启回读、Client Cook验证；Server Cook必须剥离VFX/SFX/Surface表现内容。
- 天气引擎定义、Provider目录及Niagara NiagaraSystem/SFX Definition的原生资产和跨端状态保持独立，最终以真实新手村与双客户端运行证据验收。

执行证据详见`Docs/Implementation/WeatherSourceAssetsExecution_20261010.md`。

## 一次编辑器启动批量执行方案（准备期 / 实际交付分离）

- `AuthorWeatherProductionPipeline.py`按`WEATHER_AUTHOR_PHASE`决定一个实际Editor进程中的依赖阶段。
- 默认`inspect`只列计划，不写资产。先通过现有`GamePlatformSurfaceCoreAssets` Commandlet创建MPC并用`-ValidateOnly`回读，再在真实UE Editor中设置`WEATHER_AUTHOR_PHASE=prepare`统一运行纹理导入、七个MF、两材质母版、三种VFX材质、八份项目天气Definition、两份审核蓝图和独立Review地图。
- Niagara必须由Monolith`niagara_query(create_system_from_spec)`逐个真实制作，检查Emitter/Renderer/绑定和编译，之后设置`WEATHER_AUTHOR_PHASE=vfx-definition`制作两个VFX Definition。客户端独立SFX包具备真实资源与登记后，设置`WEATHER_AUTHOR_PHASE=audio`导入SoundWave和定义。
- `VerifyWeatherUnrealAssets.py`是实际UE反射资源验收脚本：需`WEATHER_ASSET_VERIFY_MODE=verify`，逐个加载MPC、Texture、MaterialFunction、Material、NiagaraSystem、DataAsset、Blueprint和World，并校验Shader。它不是Monolith Niagara逐Emitter检验的替代，也不代表Client/Server Cook和真实联机已通过。
- 在完整Editor模块尚不能加载以前，**这些脚本均只有作者代码/静态检查交付状态，真实.uasset仍为零**。

## 2026-10-10 先代码与蓝图、最后统一测试（当前新增）

在不修改任何正式地图的前提下，先完成了可执行的真实UE编辑器资源制作脚本：

1. `AuthorWeatherSurfaceAssets.py`：目标`/GamePlatformSurface/MaterialFunctions/`下七个有实际运算连接的函数，以及`M_GP_Surface_Master`、`M_GP_Surface_Lite`两个真正连接了MPC的母材质；`WEATHER_SURFACE_AUTHOR_PHASE=functions|materials|all`决定批次，`WEATHER_SURFACE_AUTHOR_MODE=apply`才执行。
2. `AuthorWeatherVFXMaterials.py`：在`/GamePlatformVFX/Weather/Materials/`创建雨线、雪花、雨水飞溅三个实际粒子材质，要求真实PNG Texture2D已导入。
3. `WeatherNiagaraAuthoringSpec_V1.json`：为Monolith Niagara工具提供三套天气系统、近远景雨雪发射器、飞溅发射器、Shader与Niagara强度参数、原生特效裁剪/附着策略的明确生产合同。该JSON不是运行Niagara资产，系统与发射器须由真实UE Editor制作，严格禁止先创建空`.uasset`。
   - `GenerateWeatherMonolithSpecs.py`与`WeatherNiagaraMonolithPayloads_V1.json`进一步按锁定引擎中的真实`/Niagara/DefaultAssets/Templates/Emitters/Minimal`模板、`/Niagara/Modules/Emitter/SpawnRate`和粒子Spawn/Update原生模块准备Monolith接口的`create_system_from_spec`三份输入，共五个发射器。
   - `User.WeatherNearSpawnRate`、`User.WeatherFarSpawnRate`是分别传至雨/雪近远景Emitter的粒子/秒速率，天气客户端根据当前雨雪强度计算，VFX Definition的Schema已加入参数白名单。生成JSON本身不会产生Niagara资源，也不会重编Shader。真实Monolith运行结果必须`failed_steps=0`、有效Emitter/Renderer/ParameterBinding，编译、保存、回读合格才视为交付。
4. `AuthorWeatherBlueprintAssets.py`：在`/DBAWorldPack_Village/Weather/Definitions/`创建八个Server-safe天气数据资产（晴、阴、轻雨、暴雨、轻雪、暴雪、雨后、雪后）；另基于项目世界GameMode和审核Actor分别创建`BP_DBA_WeatherReviewGameMode`与`BP_DBA_WeatherReviewController`两份真实审核蓝图，须UE编译、保存与重载。
5. `AuthorWeatherVFXDefinitions.py`：待真实Rain/Snow Niagara具备有效Emitter与渲染器后，创建两个`UGamePlatformVFXAttachedDefinition`资产；VFX强度参数严格采用`User.WeatherIntensity`，并要求天气效果通过观察者ViewTarget根组件弱附着，不在世界原点播放。
6. `AuthorWeatherAudioAssets.py`：只在客户端独立`DBASFXPack_Core`已有实资产并正式登记时，才导入三段WAV，设置SoundWave循环，并创建雨/雪SFX Definition；不在Village AlwaysCook共享地图包放纯音频。雨声当前可先用已有SoundWave的请求音量映射，MetaSound强度混合后续真实制作。
7. `AuthorWeatherReviewMap.py`：在已真实加载两个审核蓝图及Surface母材质的编辑器中，生成`/Game/Development/Weather/Maps/L_DBA_WeatherReview`独立开发审核关卡，布置地面、岩石测试网格、屋顶、PlayerStart、光源及天气控制器蓝图，编译保存回读；不得自动修改Village正式地图或增加Server Cook路径。
8. `DBAWorlds`正式GameMode通过`InitialWeatherPresetId`消费上述天气DataAsset：使用平台Data的World生命周期异步Definition租约，成功后交给唯一平台天气服务应用并释放，失败保留初始天气。与自动天气表互斥，后者优先。
9. `DBAClient`本地玩家表现从平台World天气快照事件自动发现雨雪，分别通过既有`ActivateContentPack`为VFX和SFX进行独立异步预载及类型预检，成功发布目录后通知WeatherClient重发本世界当前天气，解决晚加入和资源迟加载问题。项目只用公开Weather Runtime/Client，不访问Niagara或SFX内部执行器。

### 运行顺序

**先修复Editor模块加载 → MPC真实生成/校验 → 导入九张真实Texture2D → 制作七个MF和两个Surface母版 → 制作三张VFX材质 → Monolith制作三个Niagara和发射器 → 导入项目天气DataAsset并实际编译审核蓝图 → 按内容包门禁制作SFX → 统一测试。**

所有`AuthorWeather*.py`默认`inspect`只读；它们是实际可调用的Editor API代码，不是资产本体，也未能在本轮完整Editor DLL缺失时执行`apply`。后续任何执行产生部分资产时，应保留现场进行编译/引用审查，不自动删除或覆盖，不用伪造文件填空。

天气权威API新增`UGamePlatformWeatherBlueprintLibrary`，项目审核Actor是`ADivineBeastsWeatherReviewController`（不发RPC、不复制天气事实）。通用表现增加中立浮点参数和可选弱附着组件，VFX/SFX桥各自接入，原来不带参数的请求维持默认行为。
