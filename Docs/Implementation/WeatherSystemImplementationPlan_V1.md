# 《神兽联盟》天气系统V1.0执行计划与工作台账

2026-10-10｜工作空间DivineBeastsWorkspace；正式工程Game/DivineBeastsArena.uproject；不新建项目。

## 目标

天气权威调度与客户端表现彻底分离；平台通用、项目配置、跨端职责无环；在Village完成晴雨雪状态切换、真实粒子和表面变化、双客户端一致性及服务器表现资源剥离。

## 工作拆分与验收口径

- P0：新增插件独立职责评审，确认原46→47代码插件，架构门禁同步。交付：插件规划、归属、依赖/测试，静态审计需真实执行。
- P1：通用`GamePlatformWeatherRuntime`定义、状态校验、量化契约、预设Definition，不改Data主资产真源。交付：编译、校验自动化测试。
- P2：世界天气激活、手动切换、定时加权调度、过渡重入和服务器定时器取消。交付：引擎World测试、无定时器泄漏。
- P3：单世界复制Actor、量化From/To过渡快照、迟加入服务器时间重建、Revision旧包拒绝。交付：双客户端、多PIE/DS及重连验证。
- P4：WeatherClient订阅，Surface MPC桥接及Presentation中立VFX/SFX语义；未提供资产时必须真实报告ProviderMissing和材质绑定异常。交付：真实世界客户端验证。
- P5：通过UE工具制作真MPC、母材质/函数、Niagara雨雪和SFX资源，构建实际内容目录。交付：UE资产包身份与保存/回读、Shader编译、ProfileGPU证据。
- P6：由项目DBAWorlds GameMode在权威BeginPlay激活，Village首先验收；MainArena默认固定天气，OpenWorld后续按照区域策略扩展。交付：Village实际进图及人工审核。
- P7：性能/健壮性：事件驱动、无天气持续Tick、量化状态变化复制、过渡短周期Timer、世界退出委托注销、异步取消，检查客户端及服务端Cook和资源预算。交付：可复现回归与性能报告。
- P8：三目标原生构建、自动化测试、联机与资源审计，更新中文代码注释、插件清单、总体规划、目录文档和执行日志。交付：真实命令及退出码，未通过与未执行明确标识。

## 当前完成范围

已完成首次源码写入：`GamePlatformWeatherRuntime`、`GamePlatformWeatherClient`、`DBAWorlds`世界组合、相关Target/GameplayTags/结构审计清单。此项仅表示源码与配置修改，不自动代表编译、联网或真实天气资产完成。

尚需单独验证：三目标UE构建、引擎自动化测试、真实资产生成、客户端粒子/雨雪声音、双客户端网络、Cook/Stage、性能及人工效果。新天气类型与湿润参数是通用合同，不允许把客户端视觉值用作权威摩擦/战斗结算。

## 2026-10-10 执行进度与剩余阻断

- P0：天气插件独立职责及三层边界已落地，基线47代码插件＋16内容插件；静态基线审计Passed=True，Pester架构13/13与天气隔离5/5。
- P1：天气数值、网络量化与`UGamePlatformWeatherPresetDefinition`已实现，天气测试源文件通过C++编译；`GamePlatform.Weather.Runtime.QuantizationAndTransition`自动化用例尚未在UE运行。
- P2：服务器世界手动天气、自动权重调度、状态过渡及取消代码已落地，定向编译通过；未执行World运行自动化。
- P3：单世界权威复制Actor、From/To量化快照、版本过滤、晚加入时钟重建代码已落地；Server/Editor定向编译通过，**联机/重连行为未验证**。
- P4：WeatherClient世界子系统已完成Surface参数写入和VFX/SFX中立表现提交，独立请求身份和世界取消已修复；Client/Editor定向编译通过，真实视觉资源不可用。
- P5：**未完成**。Monolith服务虽可解析，实际查询返回Unreal Editor未运行；天气插件尚无完整加载DLL，Surface真实MPC、母材质/函数及雨雪Niagara/SFX资产缺失，不创建假二进制文件。
  - 本轮补充P5源素材：真实生成9张程序化天气PNG和3份原创合成WAV，并通过SHA256与12资源形态门禁；已提供默认只读、显式授权才导入的UE Texture2D脚本。
  - 实际尝试运行MPC核心Commandlet时因为`GamePlatformServer` Editor模块缺少可加载DLL而失败；限定模块构建因其他同工作区构建互斥未取得成功DLL。本项仍为**未完成**，实际Niagara、材质、SoundWave、Definition、Catalog和人工视觉审核全部待验。证据见`Docs/Implementation/WeatherSourceAssetsExecution_20261010.md`。
- P6：DBAWorlds正式世界GameMode增加天气激活与项目默认天气/调度配置，Server/Editor定向编译通过；真实新手村地图人工验证未执行。
  - 2026-10-10后续代码增量：增加`InitialWeatherPresetId`+GamePlatformData世界期限异步Definition预载与租约取消，可由项目天气DataAsset真正驱动服务器初始天气。制作审核GameMode/Controller两份蓝图和独立Review Map的UE编辑器脚本，真实资产保存仍待引擎恢复。
- P4补充：项目本地玩家订阅世界天气复制快照，VFX/SFX分别通过已有内容包原子预载及目录发布，晚到的内容主动刷新当前天气；客户端过渡只在天气类型/终点强度关键节点刷新，不每0.1秒反复Spawn。新增代码已写入，联机与视觉表现待实证。
- P7：已落实世界Scoped生命周期、无持续天气Tick、状态变化网络复制、0.1秒过渡定时器、量化编码、取消与重入快照处理；尚无性能指标与长稳运行证据。
- P8：三个正式Target下7次关键C++定向编译全部退出码0，Pester合计18通过；**三目标完整链接/UE Automation/Cook/双客户端/人工可见雨雪未通过或未执行**。全量Editor构建因触发4178项引擎级重建被显式终止，不计通过。
  - 最新增量：在Editor目标对8个天气/项目集成相关C++文件执行`-SingleFile`编译，全部返回0；修复新增项目表现客户端include拼接及C4456变量遮蔽。新的统一脚本静态门禁再次18/18通过。真实天气资产与完整DLL仍缺失；当前验证还不包含修改后的Client/Server完整构建、Editor启动、联机、Cook或性能。证据见`Docs/Implementation/WeatherCodeBlueprintExecution_20261010.md`。

详细执行命令、证据和限制归`Game/Plugins/GamePlatform/World/GamePlatformWeather/Docs/TestingAndEvidence.md`。后续必须在编辑器实际运行且Weather模块完成正式构建链接后，依序创建真实天气资产、绑定目录与世界材质实例，运行UE自动化及双客户端网络场景；不得凭源码存在将P5/P8标记完成。

## 原有修改保护

执行开始时工作区已有未提交战斗反馈变更：`Docs/Implementation/CombatFeedbackWorkOrders_20261009.md`与`Tests/Architecture/InspectCombatFeedbackDeliveryReadiness.py`。本次天气任务不得覆盖、撤销或提交这些文件。合入前核对Git diff并分离天气变更。
