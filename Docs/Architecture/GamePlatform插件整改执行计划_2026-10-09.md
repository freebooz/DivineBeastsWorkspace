# GamePlatform 插件整改执行计划（2026-10-09）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking。本次用户已明确要求生成计划后执行，按已授权范围连续执行，不再增加计划确认环节。

**Goal:** 在当前 main/8a12bbe 基线上修复插件审查确认的源码缺陷、资源所有权和规范问题，保留尚无批准规则或真实资源的失败关闭语义，逐项记录实际验证和剩余交付条件。

**Architecture:** 复用现有 GamePlatform → MobaCommon → DivineBeasts 的单向扩展与唯一流程执行器。首先三方整合本会话已提交的554ee8a修复，保留当前main的UI/输入/资产与文档更新；随后按互不重叠的领域修复本轮缺口。Data统一资源需求，生命周期按GameInstance/LocalPlayer/World/Actor所属作用域清理，不新增平行服务。

**Tech Stack:** UE5.8.0 C++/GAS/CommonUI/Niagara/PCG、PowerShell/Pester3.4.0、CMake/MSVC原生算法回归、现有构建与Cook脚本。锁定引擎按实际Build.version校验；不修改引擎或Shared生成物以掩盖编译错误。

**Spec:** [本轮整改设计规格](GamePlatform插件整改设计规格_2026-10-09.md)、根AGENTS、现行总体规划、插件规范P13—P27。原始审查证据在原工作区Saved/Reviews/PluginAudit-2026-10-09，开始执行时归档到本隔离工作区Saved/Validation/PluginRemediation-2026-10-09/Audit。

## Global Constraints（全局约束）

- 保留46代码／机制＋16已登记内容、40个GamePlatform稳定身份，GamePlatformOpenWorld保持退休；不为补数新增或恢复插件。
- 不改变已发布模块、反射类型、资源挂载点或协议身份；基类/接口变化先做调用、资产与回退影响说明。
- 同层无环、双层依赖声明同步；Game/Client/Server/Editor目标分别验证，服务器公共装配不带DBAClient。
- 修复失败、取消、重复、旧代次、回调重入和世界退出，不使用固定成功、空提供者或伪造资料。
- 审查源码不回写原main及其他worktree；复用已附加工作区，新分支codex/gameplatform-audit-fixes-20261009。用户后续另行授权的60枚生肖图标在原main独立执行，不覆盖其他任务的修改。
- 新增或修改文件补中文职责、调用前提、线程、异常、所有权及取消说明；总体目录规划同步。
- 资产只用真实UE工具，神兽联盟UI资产仅Monolith。缺批准地图/胜利规则/预算或真实后端合同时不得自行设ProductionReady。
- Source测试在Private/Tests或明确测试模块；根Tests中的原生纯算法用例由CMake真实编译，不能冒充UE Automation。
- 每个审查ID记为已修、待实机验证、缺交付条件或有证据的非缺陷，不丢项或将待验证计为通过。
- 当前已有远程提交授权延续；不向main强推、不合并生产、不动引擎锁或生产服务。新分支是否推送以实际验证/执行记录为准，不冒充任务全完成。

## Review Focus（专项复核）

1. 两个并存World/LocalPlayer以及重复请求ID：互不撤销资源、旧句柄拒绝、终态一次。
2. 同步广播内ResetAccount/Deinitialize/关闭页面：返回后不访问已释放Scope/Transport/数组元素。
3. 持久化成功回包丢失、Revision冲突、重放队列已满：保留事件与原幂等身份，不再加一次权威进度。
4. 活跃＋待加载同时达到边界、129次PCG清理、超大存档/HTTP正文：预算先预留或接收前拒绝，历史有界。
5. 服务器实际.uproject与目标闭包、真实资产硬引用/Cook：源码声明通过和产物通过分别记录。

## Task 1：基线、历史修复三方整合与冲突复核

**Files:** 历史554ee8a涉及的Game插件、Tests/Architecture及Docs/Implementation/GamePlatformDesignRemediation；当前main在同文件的UI/输入/文档更新。
**Interfaces:** 保持当前公开服务签名的兼容使用；本轮新调用先按最终三方结果核对。
- [x] 记录原main干净、HEAD、现有隔离工作区；归档62插件审查材料。
- [x] 运行现有ValidateDesignBaseline及72项Architecture，确认当前3项失败与4独立缺依赖。
- [x] 将554ee8a在当前8a12bbe分支三方合入，不用文件整树覆盖；冲突逐段核对，保留本轮UI/资源更新。
- [x] 检查不带入旧中文FriendlyName、旧源资产或不相关后端/部署修改。
- [x] 执行Architecture、头文件/继承/命名门禁及原生回归；本轮结果不引用历史通过数。
- [x] 形成整合提交或可审查暂存差异，台账注明兼容影响与真实结果。

## Task 2：权威角色、资格、竞技World与重连

**Files:** GamePlatformGameplay/Character/AbilitySystem/Combat；DBAGameplay；MobaCommon/GamePlatformArena；DBAArena；各模块Private/Tests及对应Docs。
**Interfaces:** IGamePlatformAbilityActivationGate、ASC BindAbilityActorInfo、GameplayActive、ServerAdmission事实、竞技生命周期扩展。
- [x] 先验证旧修复是否已覆盖GW-01/02/15；补真实ASC激活拒绝与双World适配器回归。
- [x] 在实际技能CanActivateAbility/ASC路径落实Gate，并在成功准入/出生与死亡/排空时明确权威资格。
- [x] MainArena使用现有平台角色创建/项目初始化链，必要ASC/Combat/ActorInfo缺失不得Ready。
- [x] 接通已验证连接事实到竞技Roster准入；不能将客户端自报身份当票据验证结果。
- [x] 重连恢复受控Pawn或可信重新出生；失败撤销本次状态，不重放已释放世界计时器。
- [x] 对GW-11下继承建议做实际平台基类契约检查；有冲突时优先统一生命周期接口并记录迁移，不机械改反射父类。
- [x] 保持五模式人数结构；没有批准地图/胜利配置时保留NotConfigured并明确缺哪些字段。
- [x] 运行覆盖测试及模块编译，记录五模式/联机和真实资产尚未执行边界。

## Task 3：任务对账、导航、PCG、交互与AI

**Files:** GamePlatformQuest/Navigation/PCG/Interaction/AI，限定各自模块Private实现、Public必要合同及Private/Tests。
**Interfaces:** Quest持久快照与EventId；Navigation RequestId/EngineQueryId；PCG World Generation；Interaction Session。
- [x] 用无效对账快照、满队列、重复导航ID、129次生成清理和Custom回调修改Options建立失败回归。
- [x] Quest先验证后原子替换，成功接受重放后才撤销payload；重复已处理事件不盲重放。
- [x] Navigation重复ID明确拒绝或先完整取消旧请求；旧Timer不得操作新QueryId。
- [x] PCG将活跃请求与终态历史分开，清理后回收容量，仍拒绝伪造/跨世界句柄。
- [x] Interaction提交前复制稳定Option，外部回调后复查Session/目标/代次再完成。
- [x] AI过量候选裁剪改为有界一次选择；只优化已知算法成本，不声称实测收益。
- [x] 运行原生/UE回归并同步中文说明；正式Quest后端缺合同则保留准确错误，不造Provider。

## Task 4：表现解析、复合、音效、UI与MOBA事实

**Files:** 六个平台Presentation插件中真实实现；MobaPresentation；DBAClient的两个Presentation模块。不得修改Application/Input/UI业务模块或二进制资产。
**Interfaces:** 中立Context、Catalog片段/注册句柄、Data定义/软资源需求、VFX/SFX播放/终态合同。
- [x] 先核复用修改：P13精确优先/显式父回退/完整同键歧义、MOBA Context与迟到Pawn订阅。
- [x] 新回归覆盖同名跨Fragment、Composite A↔B、255Active+128Pending、栈覆盖返回与自闭页面。
- [x] Composite Steps显式纳入必需定义或预检一致性，注册/加载阶段拒绝环与缺必需子项。
- [x] 音效统一预留总实例容量；播放前/后正确绑定完成和检测失败；关停清理一次。
- [x] UI在真正离栈/结束所有权时释放租约，初始化/失活通知不造成重入失效。
- [x] 内容激活等待实际Data预载成功再发布目录；失败撤销本次句柄/租约，取消旧代次不复活。
- [x] VFX/SFX补可诊断终态及基础回退路径；不伪造缺资源播放成功。
- [x] 默认DefinitionId合法且缺真实资源时明确拒绝；Surface缺资源错误保持，采用现有Data异步入口。
- [x] 核Camera已有批准规格与同会话外其他worktree所有权：本轮不读取/覆盖别的活跃实现，保留明确阶段；未交付资源/ReviewMap单列。
- [x] 本轮任何实际UI资产修改必须Monolith身份确认、编译保存重载和Manifest；源码修复不冒充资产验收。

## Task 5：应用、输入、存档、设置与玩家服务

**Files:** Application六插件、PlayerServices六插件；DBAClient ApplicationFlow/Input/UI三模块及Private/Tests/Docs。不得修改两个Presentation模块。
**Interfaces:** 当前Input Profile拥有映射行、保存终态；Flow Retry；账号/请求代次；Equipment GrantHandle；Online安全请求。
- [x] 验证旧修复并补本輪遗漏：外部映射行隔离、PIE同ControllerId、保存失败不得Saved。
- [x] Save在全文件读取前限制实际总字节；同步Settings读路径改为明确异步/受控加载合同。
- [x] Loading/Inventory/Entitlement/Commerce广播后复查对象/代次或持有安全快照，支持回调内关闭/重置。
- [x] Equipment EndPlay取消/失效持久回调并撤销自有GAS，重复SlotId整份拒绝且不产生孤儿对象。
- [x] Progression首次/新增/删除/错误/重置与LiveOps清空玩家状态发布事件；无业务Tick弥补事件缺口。
- [x] DBAClient允许Retry时接通明确安全重启入口；默认输入缺真实Profile保留诊断。
- [x] 集合缺失/错类型按正式合同拒绝，保留旧只读快照；响应容量在接收/解析前限制。
- [x] 四服务认证装配复用Online，无长期Token和第二认证框架；缺后端路由仍报失败。
- [x] 对APP-13合并游戏线程唤醒；中文公开合同/单位/取消及实际配置说明同步。
- [x] 运行各自原生/Automation与4专项门禁，具体期望/退出码进入台账。

## Task 6：Data、Server、Telemetry及DBAServer生命周期

**Files:** GamePlatformData/Online/Server/Session/Telemetry/Debug/DeveloperTools、DBAServer及Private/Tests/Docs。
**Interfaces:** 已签发租约可验证性、HTTP生命周期、服务器Ready/Drain、Telemetry稳定服务与扩展。
- [x] Data释放历史改为不保留无限整Lease，同时保持真实重复释放/伪造拒绝和跨GI所有权。
- [x] 所有者维护按事件及有界周期执行，不再每帧扫描全部成功租约；仍处理销毁调用者。
- [x] Admission Provider关停先停止新请求、解绑/取消全部请求，回调不捕获失效裸this。
- [x] Admission/Telemetry接收正文流式限额；Telemetry无Request自强引用闭环，取消/失败终态一次。
- [x] DBAServer世界退出撤销Ready准入/心跳或进入明确失败/排空状态；新的World必须重新核对Profile。
- [x] Telemetry缓存与注册表具体实现收窄Public；保留必要扩展数据与现有消费者兼容，先核依赖。
- [x] DeveloperTools性能执行器缺生产场景保持真实Unsupported，定义扫描覆盖/编辑器成本记录。
- [x] 运行对应失败/取消/重入/跨世界回归与Client/Server模块编译；不改锁定HTTP补丁。

## Task 7：真实目标装配、服务器资源与规范同步

**Files:** .uproject、三Target、受影响.uplugin/Build.cs、DedicatedServer/FrontEndClient配置、Tests/Architecture、正式规划/目录/接口/说明。
- [x] 修复4独立漏声明，并按真实用途对ClientOnly/ServerOnly模块标明Game/Client/Server/Editor许可。
- [x] .uproject及英雄/Common纯表现入口从Server声明根排除，不误剥离批准的最小权威动画。
- [x] 用实际启用根建立Server闭包回归：不得含DBAClient/VFX/UI/Surface客户端模块。
- [x] 同步46＋16、82模块、DBAClient五模块、Boar稳定身份与真实内容阶段；历史执行数不回写。
- [x] Localization/Animation/Lobby/Village等预留状态作为存量实施决策记录，既不冒充完善也不为通过门禁造空功能。
- [ ] 全量本次受影响文件补中文责任/API/异常说明；记录未受影响中文存量，不声称全工程完成。
- [x] 总体目录规划逐项登记新增/移入文件；Public实现移动留影响和回退说明。

## Task 8：集成验证、独立复核与交付

**Files:** 现有Build/Game脚本、Tests与本计划/进度台账；验证日志只在本轮Saved独占目录。
- [x] Architecture/继承/头文件/英文命名与专项门禁全量运行；原生所有现有CMake在Debug/Release执行。
- [x] 用F:/UnrealEngine-5.8.0-release实际5.8.0工具链运行Editor/Client/Server受影响模块构建；输出目标、命令、退出码和失败根因。
- [ ] 条件满足时运行UE Automation、AssetRegistry/DataValidation、双目标干净Cook/Stage与服务器包审计；前提缺失逐项留证。
- [x] 以本轮分支完整diff、计划、审查ID台账做独立代码复核；重要发现修复后运行覆盖回归。
- [x] 最终核对审查分支无凭据/假资产/无关后端部署变更，原main与其他任务的改动未被覆盖；生肖图标任务另列范围、更新进度及剩余材料条件。
- [ ] 提交可验证源码和中文文档；按用户明确要求整合到main并正常推送，保留明确未执行验收，不将代码修复等同产品上线。

## 执行台账与回退

台账：Docs/Implementation/GamePlatformAuditRemediation/ExecutionProgress.md；每Task记录红灯/改动/绿灯/未执行，审查ID处理状态单独登记。新文件由根执行者统一更新总体目录规划，领域实现者只改归属插件文档。

回退以新分支任务提交为单位逐次revert，不覆盖main/旧worktree/资产；跨Public接口和Content登记变更先恢复消费者再恢复供给。未经确认不删除旧稳定身份或真实资产。缺批准配置、资源或服务器合同不能作为代码已完成条目，保持交付条件待满足。


## 独立复核追加验收

第一次三组复核共确认24项Important（7＋9＋8），没有确认Critical；追加复核又指出Quest提前使用变量、排空回调内完整Reset、死亡统计通知内比赛结束三项。每项都进入责任组修复，不以原生策略测试绿灯代替UObject行为验证。

- [x] Quest首次读取运行身份前不再使用尚未声明代次。
- [x] 正常停止准入允许取消通知内升级完整Reset/Deinitialize；追加实际公开调用回归源码。
- [x] 竞技死亡统计通知后和复活Timer到期核同World/Match/Adapter/玩家/Pawn代次，结束后不能重新出生。
- [x] 设置/Save同步完成、五领域关闭、Inventory终态、Equipment候选授予及严格数字/集合请求复核后再次冻结。
- [x] VFX自然结束清账、抽象Data约束、Corrected关闭、Composite必需子拒绝、UI替根/替VM、MOBA首次Travel事实/排队取消复核后再次冻结。
- [x] 对最终冻结源码执行本轮UE构建并记录真实结果，补齐逐插件处置表和独立复核结论。


- [x] Telemetry自定义Sink的Start/Shutdown/GetHealth重入保护、关闭后记录/配置拒绝、异步唤醒代次及独立上下文发布权已修，八个真实回归源码被实际目标编译且纳入最终172全部Success集合；真实HTTP/生产预算未验。

## 当前勾选边界

[x]只表示实际执行的对应范围。最终Editor5d557ce3完整目标、Client ec424675的66模块、Server b09b06b7的46模块均真实退出0；最终172唯一UE逻辑项逐项完成并Success，精确清单、日志及引擎JSON一致。24原生工程Debug/Release48配置、41唯一注册/82执行、架构Pester75/75及构建脚本28/28真实证据分别记录，不互相冒充。

本次中文人工审核覆盖新增/修改接口与关键所有权、线程、失败、取消和清理路径；全工程及并行主线新增代码的历史存量尚未逐行认证，上面的全量中文步骤仍保留[ ]。混合资产/Cook步骤只完成具名Automation，真实AssetRegistry/DataValidation、Blueprint默认子对象兼容、双目标干净Cook/Stage、三角色/五模式联机和硬件性能仍保留[ ]。

13条启动中未具名条件错误及16个带warning的成功用例真实保留，不认证全编辑器零诊断。阶段尚缺的正式后端/内容预算不造空实现；原main未提交资产和其它任务修改保留。最终源码及证据提交将正常推送main，成功回读后单独更新推送条目，禁止强推。
