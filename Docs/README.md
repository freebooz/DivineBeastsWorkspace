# 工作空间文档

更新日期：2026-09-29。以下分别列出现行规范、实施文档与历史来源；不能用目录规划或聊天中的完成描述替代真实验证结果。

## 现行规则与规划

- [全局工程规则](../AGENTS.md)：命名、中文注释、分层、权限及验证规则。
- [游戏端核心要求](Architecture/游戏端核心要求.md)：UE5.8客户端与Dedicated Server的插件化、复用、解耦、端侧权威、独立演示和人工审核核心基线。
- [业务后端核心要求](Backend/业务后端核心要求.md)：Go业务控制面的领域模块化、五薄入口、跨游戏复用、UE权威边界、契约治理、一致性、安全和真实验收核心基线。
- [解决方案总体规划](Architecture/解决方案总体规划.md)：既有产品基线与分期交付；46+N插件、三角色及可选竞技按已批准三层方案实施。
- [解决方案总体目录规划说明](Architecture/解决方案总体目录规划说明_V1.3.0.md)：正式路径、中文职责和目录维护要求。
- [设计基线整合实施规格草案](Architecture/设计基线整合实施规格草案.md)：已批准并修订为46+N的插件、三角色服务器及旧实现迁移目标；尚非实施结果。
- [设计基线整合实施计划](Architecture/设计基线整合实施计划.md)：历史执行记录；旧四DBA及45上限已被新方案替代，历史证据不回写。
- [游戏端插件三层架构实施规划](Architecture/游戏端插件三层架构实施规划.md)：本次授权方案、迁移清单、依赖边界、内容登记、回退与实际验证限制。
- [三层类继承与扩展规范](Architecture/三层类继承与扩展规范.md)：GamePlatform、MobaCommon与DivineBeasts之间的继承、接口、组件和数据扩展边界。
- [游戏端插件系统P0收敛审计](Architecture/游戏端插件系统P0收敛审计.md)：P0-1～P0-9真实审计、Definition迁移矩阵、三层继承门禁、VFX扩展点、Online/Session阻断、3A表现规格、Review Harness与Phase 1执行顺序。
- [游戏端插件清单设计](Architecture/游戏端插件清单设计.md)：当前46个代码／机制插件的层级、分类、模块端侧、已实现功能、成熟状态、验证资料和后续完善重点主台账；现行40个GamePlatform稳定身份包含新增GamePlatformSurface，GamePlatformOpenWorld继续保持退休。
- [插件开发规范](../Game/Plugins/插件开发规范.md)：UE 插件依赖、生命周期和交付门禁。
- [十二生肖技能数据驱动实施计划](superpowers/plans/2026-10-09-zodiac-ability-data-driven-implementation.md)：按现行三层与五个 DBA 代码插件规范，分阶段建设真实技能授权、数值配置、图标与技能栏自动初始化；当前仅为计划，非功能完成证明。
- [十二生肖技能数据驱动实施记录](Implementation/十二生肖技能数据驱动实施记录_20261009.md)：2026-10-09 新增技能数据定义、GAS 授权/所有者复制、客户端视图及竞技出生接口的真实代码变更，明确实际静态检查、UE/Monolith 阻断与未交付的正式技能/图标/蓝图。
- [十二生肖技能主数据及资源缺口清单](Implementation/ZodiacAbilityAssetInventory.json)：12 个稳定英雄编号和现存英雄定义文件的机器可读清单，未批准技能名称、伤害数据、技能图标和真实授权均显式为空或待验证。

- `Tests/Assets/ValidateZodiacAbilityDelivery.py`（十二生肖技能交付只读预检）：`--inventory`检查现存12英雄定义、60个已导入图标纹理与2个真实技能Widget，`--release`严格拒绝缺少12套正式技能ID/数值/界面绑定及UE三目标编译、联机、Cook的生产发布；最新结果及70项缺口记录于十二生肖技能实施台账。

## 历史决策与后续插件实施

- [神兽联盟历史对话与插件工程实现参考](Architecture/神兽联盟历史对话与插件工程实现参考.md)：历史方案演变、现行采用方式、ApplicationFlow 合同和后续插件职责。
- [神兽联盟历史对话来源索引](References/神兽联盟历史对话来源索引.md)：本轮读取七个会话、四十八轮消息的来源定位；不声称无遗漏导出全部历史。

## 当前实现与验证

- [工程缺项修复执行记录](Architecture/工程缺项修复执行记录.md)：补齐六项默认配置、源码头文件预检、原生内核测试、真实UE构建失败证据与尚未接通的项目API。
- [游戏端插件系统P0收敛审计](Architecture/游戏端插件系统P0收敛审计.md)：P0-1～P0-9的插件、Definition、继承门禁、表现规格、Review Harness和Phase 1实施顺序。
- [游戏流程与会话准入后端纵向修复设计规格](Architecture/游戏流程与会话准入后端纵向修复设计规格.md)：项目Flow、Gateway/GameServerControl、PostgreSQL准入与真实UE连接绑定的待审设计，不代表已实施。
- [Foundation M0执行进度](Implementation/FoundationM0/ExecutionProgress.md)：00→03实际断点、兼容决定与原位历史构建阻断。
- [Foundation M0实际接口](Implementation/FoundationM0/InterfaceContract.md)：Core、Data、Flow及主工程调用的已写入签名。
- [Foundation M0分项验证](Production/FoundationM0Verification.md)：目标、退出码、证据与未执行项，当前不是全部通过。
- [Foundation M0源码交付](Production/FoundationM0Delivery.md)：现有命令、资产生成顺序与剩余风险，UE目标/资产未验证。
- [GamePlatformCore](../Game/Plugins/GamePlatform/Foundation/GamePlatformCore/README.md)：平台身份、结构化错误码、结果、版本兼容区间及 Editor／Client／Server 三端模块验证；详细设计见其 `Docs/Architecture.md`。
- [GamePlatformData](../Game/Plugins/GamePlatform/Foundation/GamePlatformData/README.md)：Definition根体系、统一主资产加载、作用域租约、AssetRegistry元数据、依赖安全上限及 Editor／Client／Server 模块验证；详细设计见其 `Docs/Architecture.md`。
- [GamePlatformApplicationFlow](../Game/Plugins/GamePlatform/Application/GamePlatformApplicationFlow/README.md)：流程执行机制、节点注入、接口示例及原生／UE 验证状态。
- [GamePlatformInput](../Game/Plugins/GamePlatform/Application/GamePlatformInput/README.md)：跨游戏可扩展SemanticId/Descriptor、ProfileCompiler→CompactSlot、PC键鼠/手柄与Touch统一链、租约/重绑定/输入诊断和UI/Gameplay仲裁；旧固定技能枚举仅兼容，神兽联盟项目语义位于DBAClient。
- [GamePlatformLoading](../Game/Plugins/GamePlatform/Application/GamePlatformLoading/README.md)：DAG加载任务、Data租约、世界Ready屏障、按需低开销调度、运行诊断及 Editor／Client／Server 模块验证；详细设计见插件 `Docs/Architecture.md`。
- [GamePlatformVFX](../Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/README.md)：已有特效源码交付与待真实工程接入事项。
- [业务后端与共享代码工程化审查报告](Production/业务后端与共享代码工程化审查报告_V1.3.0.md)：后端及协议的审查证据与阻断项。
- [业务后端本地部署说明](Production/业务后端本地部署说明.md)：本地部署与验证说明。
- [文档变更记录](CHANGELOG.md)：本入口启用后的变更记录。

后续新增插件，须同步维护总体目录规划、插件细化目录、接口与验证文档，并在历史参考中记录明确的新决定及其替代关系。规范正文使用中文名称；README、CHANGELOG、AGENTS 等固定入口遵循现有工具约定。

## 原有目录索引

本目录是工作空间唯一的正式文档入口；根目录仅保留工具规则和工程配置。

| 文档 | 说明 |
| --- | --- |
| `Architecture/解决方案总体规划.md` | 三层架构、整合决定、运行流程与分期交付规划。 |
| `Architecture/解决方案总体目录规划说明_V1.3.0.md` | 当前目录、职责与维护规则。 |
| `Architecture/SolutionDirectoryTree_CN_V1.1.0.md` | 历史目录规划索引，仅用于版本追溯。 |
| `Production/业务后端本地部署说明.md` | 后端五服务的 Docker 本地开发部署、验证、排障与停止说明。 |
