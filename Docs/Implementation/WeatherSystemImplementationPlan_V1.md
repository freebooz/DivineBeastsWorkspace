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

## 原有修改保护

执行开始时工作区已有未提交战斗反馈变更：`Docs/Implementation/CombatFeedbackWorkOrders_20261009.md`与`Tests/Architecture/InspectCombatFeedbackDeliveryReadiness.py`。本次天气任务不得覆盖、撤销或提交这些文件。合入前核对Git diff并分离天气变更。
