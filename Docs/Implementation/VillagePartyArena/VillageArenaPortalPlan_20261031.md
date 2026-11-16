# SSLM 新手村竞技入口与约战 Implementation Plan

> 使用subagent-driven-development分工实现并独立审查；真实UE资产由主任务通过Monolith串行制作。用户已批准完整设计，不重复申请实施许可。

**Goal:** 五个模式门、统一面板、整队匹配及固定对手约战，复用唯一MainArena准入/返回链。
**Architecture:** 平台Interaction校验世界交互，DBAWorlds只拥有入口定义与Actor；DBAArena客户端处理模式和UI，后端在现有事务队列内增加约战预备阶段，最终进入同一Candidate/编排器。
**Tech Stack:** 锁定UE5.8/C++/Monolith，Go现有版本及Shared/OpenAPI生成工具。
**Spec:** VillageArenaPortalDesign_20261031.md（已批准）。

## 全局约束

- 当前正式工程就地增量；保留全部既有修改，不操作只读.git，不创建第二套工程/运行器。
- 单人可匹配；队伍不可拆分；五个稳定模式ID不变；UI字号10，事件驱动。
- 不使用HWebCodex。原生资产操作通过Monolith。所有构建/Cook/临时输出在工程Saved内。
- 继续500米暗场竞技地图，无辉光。只受控更新相关Village/MainArena；启动前核对真实双端战斗版本，不限帧。

## 共享接口（实现前固定）

- Gate类：`ADivineBeastsArenaEntrance`（DBAWorldsRuntime），公开`FName GetArenaModeId() const`，平台交互OptionId=`Arena.Entry.Open`。客户端按InteractionCommitted/成功结果及此Actor读取模式，不直接切图。
- `OpenMatchmakingMenu(const FString& PartyId=FString(), FName ModeId=NAME_None)`：原入口增加可选预选模式，已存在报名时不得覆写服务端锁定模式。
- 匹配命令增量action：`challenge`（targetPlayerId、partyId、arenaModeId、regionId、expectedRevision）、`challengeReply`（challengeId、expectedRevision、accept）、`challengeCancel`（challengeId、expectedRevision），均沿用clientRequestId、可信ActorContext。
- PlayerSnapshot增量可选`challenge`对象：`challengeId`、`revision`（字符串int64）、`ownerPlayerId`、`targetPlayerId`、`arenaModeId`、`regionId`、`deadlineUtc`、`canReply`、`canCancel`、`teamAPlayerIds`、`teamBPlayerIds`。未完成邀请时顶层state=`challengeInvitation`；其它现有必需字段保留。
- 邀请60秒，双方满编名单版本校验；接受后固定两队进入现有30秒逐人确认，禁止随机搜索。未邀请不分配服务器/签票。目标忙碌、重复/超时/名单变化安全终结。

## 任务与验证

### 1 后端与Shared合同

文件：Shared/Contracts/GamePlatform/OpenAPI/matchmaking-lifecycle.openapi.yaml，Backend/internal/modules/matchmaking，Backend/internal/app/{matchapi,gateway}，现有http/gRPC适配及redis事务仓储，现有生成注册表。

- [ ] 先写1v1、满队NvN固定名单、目标拒绝/忙碌/版本变化、重复提交与超时测试，运行观察RED。
- [ ] 约战保留现有事务存储、唯一预留、观察通知；抽取共同候选构造，不复制准入或编排。
- [ ] 合同真源与严格解码、生成绑定/注册表同步；邀请与迁移秘密分离。
- [ ] 对应go test、竞态及productiondeps/grpcdeps适配回归GREEN，留下结果。

### 2 客户端统一面板与约战

文件：DBAArena/Source/DivineBeastsArenaClient内OnlineMatchmakingAdapter、UIClientSubsystem、MatchmakingScreen及现有回归。

- [ ] 先写模式预选/已报名重入、邀请投影、过期/认证代次隔离回归RED。
- [ ] 实现共享接口、快速/约战切换、公开ID输入与收到邀请接受/拒绝/取消；复用同一观察、候选确认和旅行。
- [ ] 监听本地Interator的成功提交；私有依赖DBAWorldsRuntime和GamePlatformInteraction，不反向引入竞技依赖。
- [ ] UI控件合同交给Monolith作者；关闭页面不取消业务，初始快照及事件更新。

### 3 世界入口与原生资产

文件：DBAWorldsRuntime入口Definition/Actor及Private/Tests；Village世界包的五个定义、共用模型/材质和真实地图；原有竞技WBP。

- [ ] 原生测试确认合法模式/共享交互/不消耗/距离视线/无自动旅行。
- [ ] 实现GetArenaModeId；主任务勘察真实村庄并备份地图，在合适位置制作五门，共用模型与材质。
- [ ] Monolith修改原有竞技WBP，绑定真实控件，全部字号10，编译保存重载。
- [ ] 检查五门模式唯一、地面/导航可达、镜头截图、UI回读与资产引用；客户端纯表现不得进入专服。

### 4 集成与交付

- [ ] 每项实现任务完成一次独立只读规格/质量审查，修正阻塞。
- [ ] Editor/Client/Server受影响目标固定参数增量构建；原生回归及Client/Server Cook/剥离。
- [ ] 核对实际运行服务器构建身份后受控更新；两会话验证1v1，较大模式明确实际会话数量。
- [ ] 最终独立整体审查、更新记录并打开编辑器供人工检查。未完成的联机/人工/GPU验收分别报告。

## Review Focus

用户2026-10-31追加：五门共享局部门心旋涡及向中心吸收的粒子。中心膜面和单Emitter GPU粒子均为真实资产；由World作用域主LocalPlayer预载内容事务，经既有Presentation→VFX运行，每门独立取消身份。四档降低装饰密度，Low关闭空气折动；不改准入、碰撞或服务器权威。原生渲染、冷重载、生命周期回归及Client Cook必需依赖随本阶段验证。

重复提交不能生成多个约战/候选；对方邀请不能挟持不相干队伍；队长/名单/认证换代使旧回复无效；非活动页面仍能收到真实状态且无轮询；模型/纯表现剥离不能破坏服务器交互或导航。
