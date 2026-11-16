# SSLM 音频资源库扩展 V1.0（2026-10-18）

## 一、实施范围与原则

本批在既有 `GamePlatformSFX`（跨游戏音效执行）及 `DBASFXPack_Core`（SSLM客户端音频内容包）基础上，追加32段可商用CC0来源声音，保留原有9段首批音频、天气声音及项目玩法。没有复制第二套音效播放器或独立数据服务。

三层职责不变：`GamePlatform` 处理音频加载、并发、距离与生命周期；`MobaCommon` 产生通用战斗表现事实；`DivineBeasts` 只持有具体项目声音、可复用数据定义与音频语义。纯客户端声音不得硬引用进入Village共享服务器地图，也不作为伤害、命中或技能成功的前置条件。

音频来源为真实网络公开下载，已保留原始压缩包摘要、单个音频 SHA-256、作者、公开网页与每个音频的原始归档路径；统一详见：

`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/Docs/SSLM_AudioLibraryExpansion_20261018.json`

本批素材全部选择原网页标注 CC0 1.0 的开放声音，**仅作为音效设计底稿/原型资源**，不表示通过最终3A风格、混音、听力舒适度或发行授权法律审核。

## 二、真正导入的32个原生声源

| 分类 | 数量 | 具体内容及预定用途 |
|---|---:|---|
| Footsteps（脚步） | 6 | 落叶A/落叶B、砂砾、泥地、石面、木板：新手村和开放世界的地表材质脚步 |
| Water（水体） | 2 | 小型水花两种变体：水面接触、飞溅等 |
| Ambience（环境） | 3 | 溪流A/溪流B循环、鸟鸣：湖滨、瀑布过渡、桃林环境细节 |
| World（世界交互） | 3 | 木门关闭、木材破裂、木门开启：村庄建筑与可交互物件 |
| Combat（战斗） | 7 | 木/金属冲击、两种兵器碰撞、轻/重挥动、匕首切割 |
| Creatures（生物） | 5 | 嚎叫、犬吠、吼叫、低吼、狼人生物声：仅作为狼怪物等生物的选用参考 |
| UI（交互界面） | 3 | 翻书、金币、重金币：背包、图鉴、交易或领取反馈 |
| Spells（法术） | 3 | 奥术施法、治疗、传送：开发占位技能音效，后续可复合MetaSound处理 |

合计 **32 SoundWave（声波资源）＋32 `UGamePlatformSFXDefinition`（音效定义）＋4个 SoundConcurrency/SoundAttenuation（并发/衰减）原生资产**。新增脚步/生物/环境并发策略与三维环境衰减，不改变已有9段原型音效使用的声学策略。

新增真实引擎资源目录：

- `/DBASFXPack_Core/Audio/Library/<分类>/Waves/`：客户端原生 SoundWave。
- `/DBASFXPack_Core/Audio/Library/<分类>/Definitions/`：数据定义；每个定义通过 `SFXRuntime` Bundle 异步加载声音及策略。
- `/DBASFXPack_Core/Audio/Shared/Policies/`：公共并发和距离衰减策略。
- `.../DBASFXPack_Core/SourceArt/Audio/Library/`：非烘焙源音频，附来源与哈希。

## 三、命名与调用示例

例一，治疗施法：

- 声波资产：`S_DBA_Spells_Healing`
- 定义资产：`DA_DBA_SFX_Spells_Healing`
- 逻辑ID：`presentation.dba.audiolibrary.spell.healing@1`
- 事件标签：`Presentation.SSLM.AudioLibrary.Spell.Healing`
- 定义播放空间：World（三维空间）。

例二，桃林落叶脚步：

- 声波：`S_DBA_Footsteps_LeavesA`
- 定义：`DA_DBA_SFX_Footsteps_LeavesA`
- 逻辑ID：`presentation.dba.audiolibrary.footstep.leavesa@1`
- 事件标签：`Presentation.SSLM.AudioLibrary.Footstep.LeavesA`。

通过 `IGamePlatformSFXService::Get(WorldContext)` 获取当前世界播放器，传入 `FGamePlatformSFXRequest` 的 `RequestId`、`DefinitionId`、`Location`，即可由平台异步查询及播放注册的定义。 `Play` 返回 `Queued` 仅表示受理加载，不等于已经听到声音。

**关键状态限制：** 本批完成的是音频库和GameplayTags/AssetManager登记，**尚未为32个语义事件建立激活中的GamePlatformPresentation Catalog片段**，没有将其接到特定角色脚步、技能、狼怪物、背包按钮或桃林关卡。将标签注册到 `DefaultGameplayTags.ini` 不会自动触发任何播放。后续应按Category拆分项目Catalog内容片段、采用 `ActivateContentPack`（带预载/撤销门禁）按地图或功能订阅激活；不得一次将所有环境声音无条件设为AutoPlay。用户可先在Unreal编辑器 Content Browser（内容浏览器）启用Show Plugin Content（显示插件内容），进入上述 Waves 目录人工逐个试听。

建议下一阶段优先关联：
1. 植被/泥土/石块/木板地表材质 → 对应Footstep语义，直接按地表配置选择而非随机跨材质播放；
2. 新手村桃林及湖边音频区域 → 鸟鸣、流水，环境声应按区域位置及声源距离执行；
3. 项目狼怪物 → Howl/Bark/Grunt，不随每次行为都强行发声；
4. 既有GAS法术表现 → ArcaneCast、Healing、Teleport，先确认本地预测与服务器确认不重复；
5. 背包、图鉴与交易UI → BookFlip/Coins/CoinsHeavy，经统一UI中立表现请求触发。

## 四、可追溯制作与验收结果

- 资源原始提取清单：`.../DBASFXPack_Core/Docs/SSLM_AudioLibraryExpansion_20261018.json`（32条、作者/出处/CC0/归档源SHA/单资源SHA）。
- UE原生生成器：`Tools/Unreal/Audio/ImportAudioLibraryExpansion.py`（默认inspect检查，显式 `SSLM_AUDIO_LIBRARY_MODE=apply` 才变更资源；已存在资源不覆盖其他类型）。
- UE原生冷回读验证：`Tools/Unreal/Audio/ValidateAudioLibraryUE58.py`。
- 静态安全校验：`Tools/Unreal/Audio/TestAudioLibraryStructure.ps1`。
- 首轮故障：独立PythonScript命令进程启动时，`/DBASFXPack_Core`的注册表未同步扫描，导致既有音频衰减策略加载失败。已在导入器开头加入注册表同步扫描，并重新执行成功。
- 第二次原生音频导入：`Game/Saved/Logs/SSLM_AudioLibraryExpansion_Retry_20261018.log`，原生命令退出码0，日志 `SSLM_AUDIO_LIBRARY_IMPORTED_OK 32`。
- 独立UE5.8冷重载：`Game/Saved/Logs/SSLM_AudioLibraryColdVerify_20261018.log`，原生命令退出码0，日志 `SSLM_AUDIO_LIBRARY_COLD_VERIFY_OK 32 4`。
- 结构门禁：`SSLM_AUDIO_LIBRARY_STATIC_VERIFY_OK 32 4`。
- 当前未进行：引擎有声运行试听、完整Client/Server编译、Dedicated Server最小Cook、5v5 Audio Insights声学性能与真实玩家体验验收。

## 五、进一步优选资源

1. Fab `Free Modular Magic SFX`（多种元素魔法、MetaSound示例）可用于正式技能音效升级，但必须通过合法Fab账号取得文件，并遵守CC BY 4.0署名条款。不得抓取YouTube试听片段代替授权下载。
2. Sonniss GDC 2026大型专业音效包体积约7.47 GB，后续宜分场景/材质筛选候选样本，而非整包不加区别地Cook到客户端；保留原许可及禁止AI训练等条款。
3. 当前CC0原型适合作为基础动作、战斗和世界声音；正式制作应通过 MetaSounds（程序化音频）、Audio Modulation（音频调制）、Submix（子混音）统一执行品质审查和5v5性能预算控制。

## 六、不可越过的工程门禁

真实资源交付必须取得：作者许可/来源复核、WAV/OGG试听与峰值检验、UE原生资产冷回读、项目目录映射与Runtime激活、关卡真实声音、Server内容隔离、性能报告、人工声学验收。当前阶段只取得前述资源身份、引擎资产创建与冷回读证据，未宣称“可正式发布3A声音”。
