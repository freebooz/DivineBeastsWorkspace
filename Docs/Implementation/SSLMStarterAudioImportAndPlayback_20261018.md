# SSLM 首批背景音乐、环境、按钮与魔法音效接入记录

日期：2026-10-18｜工作空间：`DivineBeastsWorkspace`｜引擎：UE5.8

## 目标与真实资产

本批通过许可明确的可商用 CC0 文件，导入九份声音及对应九份 `UGamePlatformSFXDefinition`，并制作五份原生并发/衰减策略。音频原始文件保留于 `DBASFXPack_Core/SourceArt/Audio`；实际 UE 音频资产全部在 `DBASFXPack_Core/Content/Audio`，属于仅客户端表现内容。所有音源的来源、许可、SHA256在本内容包 `Docs/SSLMStarterAudioSources_20261018.md`。

对应3类按钮音（Click/Confirm/Cancel）、3类技能音（Frost Cast/Frost Impact/Fire Impact）、2首场景音乐（Login/Village）与1份森林氛围。原始文件目前是CC0原型母版，尚不是经过专业音频设计、混音及5v5压测的3A最终音频。

## 运行时调用路径

```
客户端事件/场景状态（第三层）
  -> DivineBeastsPresentationClientSubsystem（项目音频意图）
  -> GamePlatformPresentation（既有中立语义目录）
  -> ProviderChannel=SFX（唯一平台播放执行提供者）
  -> GamePlatformSFX（已有World级播放器）
  -> GamePlatformData（Definition与SFXRuntime软资源租约）
  -> Unreal Engine AudioComponent / USoundWave
```

新增 `BuildStarterAudioFragment` 将九条语义注册至 `Presentation.SSLM.Audio.*`，对应九个 `presentation.dba.audio.*@1` 逻辑定义。更新 `DefaultGameplayTags.ini` 及已有唯一 `PrimaryAssetTypesToScan` 主资产扫描配置。内容包激活调用 `ActivateContentPack`，全部九条资源定义成功租约预载后才一次发布；任何定义失败均不声称目录就绪。只在客户端World启动，独立于服务器玩法判定。

### 已接线的首批效果

- 进入前端地图 `L_DBA_FrontEnd` 或角色预览工作室：播放登录配乐；
- 进入新手村地图 `L_Village_Start`：播放村庄配乐和森林氛围循环；
- 点击登录按钮：向中立SFX提供者提交 Click 音效；
- 开发占位法术投射事件 Frostbolt/Fireball：在合法弹道事件发出冰系起手、冰系/火系命中音效；它们不是12生肖96技能的最终配置，也不替代真正GAS施法阶段音频设计。
- World结束、更新及账号切换：先发送旧循环声Cancel并撤销音频目录和数据租约，避免资源跨场景泄漏。

首期2D背景音乐由原有SFX World服务执行，无节拍对齐、跨地图持续和真正Music Director；后续正式版本应交由独立音乐导演实现。环境森林声当前是世界级2D氛围床声，不等于单独的3D森林声景。技能音效已配置真实3D衰减（内圈3米，渐弱区约24米）和专用并发资产；UI/音乐/环境分别具有自己的声音并发策略。

## 资源作者工具与验证

- `Tools/Unreal/Audio/ImportStarterAudio.py`：UE Editor真实导入，默认只读；设置环境变量 `SSLM_AUDIO_IMPORT_MODE=apply` 才实际导入。保持SoundWave以`S_`命名、Definition以`DA_`命名；脚本重复执行时保留已有合规SoundWave，避免无意义写入。
- `Tools/Unreal/Audio/ValidateStarterAudioUE58.py`：独立编辑器进程对9个声音、9个Definition、5份策略资产进行冷回读验证。不能证明已通过扬声器听见。
- `Tools/Unreal/Audio/TestStarterAudioStructure.ps1`：独立的静态文件存在/源许可指纹/配置/代码检查。
- `Game/Saved/Logs/SSLM_AudioColdVerify_20261018.log`：最近一次编辑器冷回读输出，成功标志 `SSLM_AUDIO_COLD_VERIFY_OK 9 5`。
- `Game/Saved/Logs/SSLM_AudioPolicies_Final_20261018.log`：UE Editor作者输出，成功标志 `SSLM_AUDIO_IMPORT_OK 9`。

## 人工试听与发布门禁（尚未全部完成）

1. 在 Content Browser 显示插件资源，打开 `DBASFXPack_Core/Audio`；逐个SoundWave人工试听，评价干净程度、声音辨识、循环边界与整体风格。
2. 完成 Editor/Client真实增量编译；若Foundation构建锁被别的合法作业占用，保留锁并报告阻塞，不能伪称编译成功。
3. 启动客户端进入Login、Village；确认按场景播放、暂停/停止、按钮单击仅发一声。
4. 验证寒冰箭的起手和命中，火球的命中，客户端预测去重；镜头靠近/远离时音量变化及大量并发抢占。
5. 进行 Dedicated Server最小Cook审计，排除全部ClientOnly声音uasset（不能仅凭uplugin属性推断完毕）。
6. Audio Insights 测量5v5音频声源、Render Thread、CPU、音频流缓存；由人工最终签署资源听觉验收。

### 重要限制

- 本批没有绕过Fab登录/购买权限，也没有抽取YouTube预览音频。
- 项目登录主界面的其他按钮需通过 `PlayStarterUISound`（可由蓝图调用）逐一接入；目前确认改动的是Login按钮。
- 所有客户端内容均在原有ContentPack中，不将音频硬引用加入Village共享权威地图。
- 资源完成冷回读不代表Client/Server编译、真正PIE播放、5v5性能、许可法律审核全部通过。
