# SSLM全插件审查台账与阶段整改（2026-10-13）

工作区：`E:/poject/feebooz/DivineBeastsWorkspace`。另一个HWebCodex工作区的Windows执行隧道仍离线，两个检出未经同步校验；本报告所有修改、审查和后续编译结果必须只归属此CWebCodex检出。

## 审查口径

已逐一读取 **47份代码插件描述、86份模块Build.cs（模块构建规则）、18份内容包描述**，共65份插件描述。检查内容包括：技术身份、目录/英文显示名、重复声明、模块构建规则、平台→MOBA→SSLM三层依赖、Client（客户端）/Server（专服）/Editor（编辑器）目标类型、模块依赖环。另抽样审查World、PCG、VFX、UI、Telemetry等有状态/高频源码，搜索同步加载、世界内Actor全局扫描和Tick热路径。**静态审查不等同于每个模块完整功能审计、运行性能测量或实际Cook验收**。

对全部86份模块构建规则执行依赖解析，在此次修复后，未发现重复声明、依赖本地模块却缺插件声明、跨层反向模块依赖、Runtime/Server直接链接ClientOnly/Editor模块或静态依赖环。既有专项目录 `PluginCompletionDemoAudit_20261012.md` 含各模块成熟度与Demo历史证据，不能冒充本轮复验。

## 已实际保存的修改

| 编号 | 插件/模块 | 发现的问题 | 已执行的整改 |
|---|---|---|---|
| A01 | `GamePlatformVFX`（平台特效） | `.uplugin`重复声明`GamePlatformCore`（平台核心） | 删除冗余条目，保留稳定插件身份 |
| A02 | `DivineBeastsPresentationRuntime`（SSLM表现合同） | `GamePlatformCore`同时作为公开和私有链接依赖 | 删除重复的Private声明，保留Public接口 |
| A03 | `GamePlatformWorld`（平台世界） | 同一Ticker中`Streaming->Tick()`被调用两次；无事件也复制订阅映射；回调Stop后可能继续派发 | 统一在Refresh内采样流送，空事件不拷贝订阅，停止时中断本轮派发 |
| A04 | `PluginCompositionAudit.psm1`（架构审计器） | 重复的插件依赖、同目标Public/Private重复模块依赖未拒绝 | 补充拒绝逻辑和2项Pester负例 |
| A05 | `ValidatePluginDeliveryManifest.py`（插件交付清单） | 缺少全量英文显示名称、插件目录、同名ModuleRules和重复依赖校验 | 增加65插件描述/模块源码路径一致性检查 |
| A06 | `GamePlatformPCG`（平台PCG运行） | 无请求时常驻50ms Ticker，循环枚举订阅者 | 首个有效请求启动Ticker，全部清理后停止；状态脏时才生成订阅快照 |
| A07 | `GamePlatformWorld`（区域观察者） | 周期内复制观察者数组 | 保留原位更新，移除无必要拷贝 |
| A08 | `GamePlatformQuest` / `GamePlatformWeather`（任务/天气） | 任务源码重复引入UWorld，天气Actor重复设置复制开关 | 去除重复声明 |
| A09 | `GamePlatformWeatherRuntime`（天气权威调度） | 同步通知期间重配计划可能使旧条目悬空、旧定时器覆盖新定时器 | 计划值拷贝、延迟前置、ScheduleGeneration代次保护和UE回归源码 |
| A10 | `GamePlatformWeatherClient`（天气客户端） | 旧取消回调可误清理新请求、过期快照可重新设置Timer | SnapshotApplyGeneration、显式旧请求身份取消以及过期检查 |
| A11 | 总体目录规划文档 | 嵌套Git冲突标记未清理 | 保留两侧章节、移除标记并登记新台账 |

修改采用`read_revision`（读取版本）保护的原文精确替换。保留`GamePlatformArena`历史技术标识及已有资源引用；不做无证据的批量重命名。用户未授权停止其他并行任务，因而不修改它们正在编辑的后端、背包、Demo与材质内容。

## 47个代码插件的逐项风险核对

下表每行均已完成描述和构建规则层面的结构检查；“重点风险”表示**后续需真实用例确认的关注点**，不是已经证实的软件缺陷。“已修”只对应上节具体变更。

| 代码插件 | 后续性能/逻辑/规范关注点 | 本轮结构状态 |
|---|---|---|
| `GamePlatformApplicationFlow` | 登录/切换场景重入、失败回退 | 静态核对，无新增已证实问题 |
| `GamePlatformInput` | 输入上下文、设备切换与焦点回收 | 静态核对，无新增已证实问题 |
| `GamePlatformLoading` | 加载取消、超时和重复完成 | 静态核对，无新增已证实问题 |
| `GamePlatformLocalization` | 历史为模块壳；语言切换与资源尚未闭环 | 静态核对，无新增已证实问题 |
| `GamePlatformSave` | 持久化失败恢复、原子写入 | 静态核对，无新增已证实问题 |
| `GamePlatformSettings` | 服务端策略与设备设置边界 | 静态核对，无新增已证实问题 |
| `GamePlatformDebug` | 生产模式关闭调试入口 | 静态核对，无新增已证实问题 |
| `GamePlatformDeveloperTools` | Editor限定装配与审核取证 | 静态核对，无新增已证实问题 |
| `GamePlatformTelemetry` | 高负载背压、线程安全、丢包 | 静态核对，无新增已证实问题 |
| `GamePlatformCore` | 身份与错误码兼容性 | 静态核对，无新增已证实问题 |
| `GamePlatformData` | 唯一来源/资源租约并发与内存驻留 | 静态核对，无新增已证实问题 |
| `GamePlatformLobby` | 历史为模块壳；大厅业务未闭环 | 静态核对，无新增已证实问题 |
| `GamePlatformVillage` | 历史为模块壳；通用新手引导未闭环 | 静态核对，无新增已证实问题 |
| `GamePlatformAI` | 服务端感知/寻路及可见性 | 静态核对，无新增已证实问题 |
| `GamePlatformAbilitySystem` | 权威授权、撤销、代次 | 静态核对，无新增已证实问题 |
| `GamePlatformAnimation` | 角色动画取消、LOD与客户端资源 | 静态核对，无新增已证实问题 |
| `GamePlatformCharacter` | 身份切换、旧异步回调 | 静态核对，无新增已证实问题 |
| `GamePlatformCombat` | 伤害结算、网络复制与反作弊 | 静态核对，无新增已证实问题 |
| `GamePlatformGameplay` | 服务端准入、生命周期 | 静态核对，无新增已证实问题 |
| `GamePlatformQuest` | 已清理重复include；进度、奖励幂等和回滚仍需专服联调 | 源码整理已修，业务待验 |
| `GamePlatformOnline` | 鉴权、超时、重试和失败封闭 | 静态核对，无新增已证实问题 |
| `GamePlatformServer` | Agones就绪、排空和离线 | 静态核对，无新增已证实问题 |
| `GamePlatformSession` | 旅行、重连和代次一致 | 静态核对，无新增已证实问题 |
| `GamePlatformCommerceUI` | 交易界面不得成为业务权威 | 静态核对，无新增已证实问题 |
| `GamePlatformEntitlement` | 权益缓存和撤销传播 | 静态核对，无新增已证实问题 |
| `GamePlatformEquipment` | 装备、背包与战斗事务 | 静态核对，无新增已证实问题 |
| `GamePlatformInventory` | 并发背包事务；其他会话正在修改 | 静态核对，无新增已证实问题 |
| `GamePlatformLiveOps` | 活动时间口径与失效处理 | 静态核对，无新增已证实问题 |
| `GamePlatformProgression` | 角色成长权威与重复事件 | 静态核对，无新增已证实问题 |
| `GamePlatformCamera` | 镜头、设备适配、相机震动 | 静态核对，无新增已证实问题 |
| `GamePlatformPresentation` | 表现请求与弱引用资源寿命 | 静态核对，无新增已证实问题 |
| `GamePlatformSFX` | 音频池、音量控制与试听 | 静态核对，无新增已证实问题 |
| `GamePlatformSurface` | 材质实例、雪苔藓湿润层与Cook | 静态核对，无新增已证实问题 |
| `GamePlatformUI` | 十域页面、资源异步、输入交接 | 静态核对，无新增已证实问题 |
| `GamePlatformVFX` | 已修重复插件依赖；GPU预算/池化需实测 | 已修并静态复核，运行待验 |
| `GamePlatformInteraction` | 资格检查、交互重入 | 静态核对，无新增已证实问题 |
| `GamePlatformNavigation` | 动态障碍、服务器寻路 | 静态核对，无新增已证实问题 |
| `GamePlatformPCG` | 已修空闲Ticker和订阅拷贝；生成、输出回收与高密度负载仍需引擎实测 | 已修改源码，UE回归待验 |
| `GamePlatformWeather` | 已修权威/客户端回调重入与请求取消身份；仍需双端同步和GPU实测 | 已修改源码，回归待执行 |
| `GamePlatformWorld` | 已修重复采样/关闭后派发；需UE回归 | 已修并静态复核，运行待验 |
| `GamePlatformArena` | 通用竞技生命周期、五种人数模式 | 静态核对，无新增已证实问题 |
| `MobaPresentation` | 竞技状态表现与跨世界回收 | 静态核对，无新增已证实问题 |
| `DBAArena` | 项目竞技准入、结算与出生 | 静态核对，无新增已证实问题 |
| `DBAClient` | 已修公开/私有重复依赖；客户端视觉需回归 | 已修并静态复核，运行待验 |
| `DBAGameplay` | 生肖技能装配、正式技能资源 | 静态核对，无新增已证实问题 |
| `DBAServer` | 专服准入、背包可信事件与Agones | 静态核对，无新增已证实问题 |
| `DBAWorlds` | Village到OpenWorld/竞技跨世界上下文 | 静态核对，无新增已证实问题 |

## 18个内容包逐项登记

18份`.uplugin`均可解析、`CanContainContent=true`、不含代码模块；这不能证明`.uasset`可打开、引用完整或Server Cook正确排除纯表现资源。

| 内容包 | 必须继续验证的能力 |
|---|---|
| `DBAContentPack_Common` | 公共原型模型、材质与双端Cook隔离 |
| `DBAHeroPack_Rat` | 生肖鼠专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Ox` | 生肖牛专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Tiger` | 生肖虎专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Rabbit` | 生肖兔专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Dragon` | 生肖龙专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Snake` | 生肖蛇专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Horse` | 生肖马专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Goat` | 生肖羊专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Monkey` | 生肖猴专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Rooster` | 生肖鸡专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Dog` | 生肖狗专属美术/Profile/技能图标与Cook |
| `DBAHeroPack_Boar` | 生肖猪专属美术/Profile/技能图标与Cook |
| `DBAFrontEndPack` | 前端登录/角色工作室仅Client及Editor |
| `DBAPresentationPack_Core` | 公共特效资源、Niagara与Server Cook隔离 |
| `DBASFXPack_Core` | 项目音效/混音及Server Cook隔离 |
| `DBAUIPack_Core` | 界面蓝图、统一主题、移动适配 |
| `DBAWorldPack_Village` | 新手村权威地图/出生/PCG/专服Cook |

## 未完成项与统一验证顺序

1. 本次新增门禁和既有三目标闭包：先运行`python Tests/Architecture/ValidatePluginDeliveryManifest.py`、`Tests/Architecture/TestEnglishProjectNames.ps1`及`PluginCompositionAudit.Tests.ps1`，并人工核对`git diff --check`与所有本次源码差异；若失败继续修复。
2. **全部代码/配置修改完成后**统一编译Editor、Client、Server三目标。记录真实UE5.8引擎路径、UBT命令、退出码、构建产物/receipt/源码快照。严格不复用另一工作区的历史编译作为本次成功证明。
3. 构建通过再运行World（世界）生命周期和关闭中事件派发、PCG（程序化生成）、VFX（视觉特效）、UI（界面）、GAS（技能授权）、后端准入、Inventory（背包）的真实UE回归。
4. 完成Client/Server Cook及物理包资源清单，进行Village出生与技能加载、双客户端重连、五种竞技模式、WorldPartition、材质和VFX的人工与性能测量；测量需标注机器、地图、配置、样本、耗时和内存，不能凭“优化了循环”推定帧率收益。

截至文档创建，本机Runner共有**4个运行任务、6个排队任务**，均与现存并行改动有关；为了不抢占或覆盖并发任务，**本轮尚未启动构建、UE自动化或Cook**。首次结构核对不等于正式交付。过往审查中`GamePlatformLocalization`、`GamePlatformLobby`、`GamePlatformVillage`为模块壳，不能标注完成；OpenWorld/MainArena正式地图、十二英雄技能真实专服出生及所有目标设备视觉验收仍是明确缺口。本次修改均尚未提交或推送。

执行窗口复核：尝试提交一次独立Python清单/JSON静态检查（Job `wc_job_JK1uy3G_SrYxQ_71`），因执行槽位持续占满，Runner状态始终为`agent_queued`（等待队列），最终本会话主动停止，返回`stopped / not_started / exit -1`；**没有运行任何验证命令**。因此本轮结构图检查由只读读取86份Build.cs后的静态解析得出，不能代替Python脚本、Pester、UBT、UE自动化或Cook原生结果。

### 2026-10-14 续审：源码已保存、验证待执行

本轮A06～A11新增整改：PCG在首个合法请求后启动50ms Ticker（周期回调）、所有请求清理完成后停止；World区域观察者避免周期数组复制；Quest与天气Actor删除重复声明；天气服务端以`ScheduleGeneration`（计划代次）拒绝同步事件通知中的旧计划覆盖、避免持有可能失效的数组元素指针；客户端以`SnapshotApplyGeneration`（快照应用代次）拒绝过期表现提交，并先取出旧VFX/SFX请求身份再取消，不误清理新请求。

新增`GamePlatform.Weather.Runtime.ReentrantScheduleReplacement`（天气计划重入替换）UE自动化测试源码：广播期间由旧15秒天气计划切换到新1秒计划，验证旧回调不能覆盖新定时器。已进行源码只读分隔符配对检查，关键C++文本平衡；**这不是编译或测试通过证据**。已修复`Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`中嵌套的Git冲突标记并完整保留分支文档段落。

此工作区仍有其他会话长期Job占用4个执行槽位，另有6项排队；本次没有停止它们。当前所有Editor/Client/Server构建、Pester/Python原生门禁、UE Automation、Cook、联机、人工与性能验收均未执行，不可标为完成。

