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
