# 审查整改范围、成熟度与迁移边界

日期：2026-09-30。依据是真实源码与逐项F01—F30审查；执行计划、状态和实际验证见同目录ReviewSpecification.md、ExecutionProgress.md。以下为本轮完成标准，不能将源码修复等同生产产品完成。

## 五个机制预留身份（F19）

| 插件 | 真实现状与后续门禁 |
| --- | --- |
| GamePlatformLocalization | 二期ClientOnly模块壳，无本地化服务实现；沿用UE原生文化/文本机制规划，未独立批准前不建空子系统。 |
| GamePlatformCamera | 0.1.0模块注册壳；已有2026-09-29方案A规格和独立计划，尚未实现。按本地玩家作用域、UE原生相机适配及Data租约另行实施。 |
| GamePlatformAnimation | 一期关键能力未完成；Runtime/ClientOnly稳定身份保留，权威Montage/RootMotion时序不得依赖可选表现成功。现有独立规格不等于实现。 |
| GamePlatformLobby | 条件保留，无真实通用体验消费者；Lobby属于OpenWorld体验，不能增设服务器角色/目标。须先证明既有World/Flow/UI/Party不能表达的独立职责。 |
| GamePlatformVillage | 条件保留，无通用新手机制实现；Village正式服务器角色继续由既有World/Gameplay/Quest/Interaction/AI等组合，不以模块壳冒充教学训练已交付。 |

上述README在执行基线已有真实状态说明，本轮复核并将其纳入统一矩阵，不创建空框架或擅自更改40个GP身份基线。后续任何退休/合并须独立影响检查与基线迁移。

## 业务后端与生产成熟度（F20）

Equipment、Entitlement、Commerce、Quest、Progression、LiveOps的UE Port/本地状态/协议适配不证明后端数据库、幂等事务、事务发件箱、账本、支付验证、退款或真实履约完成。本轮修正失真的README及交付说明，旧路径/迁移/Outbox说明标明为历史设计或删除错误完成宣称；没有创建生产后端或执行经济操作。真实后端接线、网络联调和故障恢复仍须独立验收。

Session及SFX在当前基线已有真实实现说明，本轮保留其端到端/产物验收边界，不用旧审查时间点的状态覆盖新实现。Settings当前仍没有生产设置描述符Provider，四个玩家服务也需要项目装配Online构造；这些不会通过固定成功冒充可用。

## 公开API与内容兼容迁移

1. 四个玩家服务HTTP transport改用当前GI的Online子系统，不留存Token。旧(URL,Token)构造保留UE_DEPRECATED标记但拒绝配置；全Game源码没有旧构造生产消费者，外部集成须显式改新构造。
2. Data新增普通ResourcePaths租约，不更改HeroDefinition/BehaviorTree/Blackboard/Widget主资产身份。租约必须由服务签发，IssuerProof为实例内防篡改摘要；旧手工构造或跨实例持久化租约拒绝。旧Definition方法与枚举编号保留。释放只结束本调用者需求。
3. 平台HeroLoader与项目Catalog保留旧RequestDefinition兼容入口及明确开发回退；角色组件、真实创建草稿流程、AI资源与竞技预热使用新作用域入口，缺真实资源明确失败。Creation Provider新增默认拒绝的scoped接口，旧提供者须迁移，不能静默恢复进程加载。
4. Input原生SaveSettings返回void，本轮把“提交保存”与“已验证落盘”分开：bPreferencesSaveSubmitted表示提交，bPreferencesSaved保持false；没有落盘证据不能报已保存。
5. VFX Composite现要求每个Steps的DefinitionId显式列入RequiredDefinitions；旧内容须补依赖边并经Editor/Data验证，缺边拒绝，不新增第二个解析器或改主资产ID。新终态枚举追加，不重用既有值。没有实际资产改动。
6. 平台技能所有原生GAS激活入口查询当前Avatar的Gate，缺Gate失败关闭；项目玩家Gate读取真实Ready/Active/代次，AI采用独立权威策略。现有玩家宿主尚无正式BindAbilityActorInfo调用者，不能宣称实际技能授予和端到端战斗完成。
7. 模块Provider仅注册机制；竞技项目扩展按真实GameMode保存独立Adapter/World资源租约并在WorldCleanup清理，同进程第二个世界不得覆盖第一个世界。

## 中文说明人工检查范围（F30）

本轮检查受影响文件的公开API、关键私有流程、失败/取消/权威/线程/资源所有权与测试目的。代表性边界包括四Online Transport、Equipment退出、Settings工厂/上下文、Loading重入、Save读取预算、Data签发/普通资源、Server准入关闭/接收、AI/角色初始化、GAS门禁、导航身份、Quest快照/重放、PCG累计容量、表现解析、VFX/SFX终态与UI/Surface生命周期。各插件专属DesignRemediation及API文档给出对应说明与剩余边界。

其余存量未做全量人工中文内容审计，不宣称全部插件或整个工作空间已满足AGENTS中文注释规范。自动查到中文字符、文件头或注释数量不构成业务含义完整的证明；未来变更仍须按AGENTS逐路径补齐。生成代码、UE资产和第三方引擎文件未人工改写。

## 回退与验证

所有变更在独立codex/gameplatform-design-remediation分支；回退以本分支提交整体revert，保持共享Data接口及消费者、Gate及组合根、描述依赖和文档同批恢复。不能仅撤销签发证明或消费者以制造混用，也不能覆盖原main工作区。原始开发兼容入口不等于生产回退策略。

真实检查结果和未执行项统一写ExecutionProgress，Saved日志是实际运行证据。源码装配、Native与Pester不证明UE运行、网络、GC、干净Cook或服务器表现剥离；没有对应结果不得提升成熟度。
