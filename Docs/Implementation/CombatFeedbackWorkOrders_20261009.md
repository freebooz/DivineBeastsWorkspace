# 战斗打击反馈系统 P0—P8 执行工单与验证门禁

日期：2026-10-09  
唯一工作区：DivineBeastsWorkspace（神兽联盟工作空间）  
执行依据：`Docs/Implementation/CombatFeedbackExecutionPlan_20261009.md`、`AGENTS.md`、插件开发规范及真实UE5.8源码。  
说明：本工单的“源码已写入”不代表真实编译、资产、Cook、多人联机或人工手感验收；只有实际工具日志才能升级状态。

## 总体架构

依赖方向严格为 **DivineBeasts（项目层） → MobaCommon（MOBA通用层） → GamePlatform（平台层）**。

平台：GamePlatformCombat（权威结果）、GamePlatformPresentation（中立请求/Profile）、GamePlatformAnimationClient（视觉局部顿帧与Overlay闪白）、GamePlatformCameraClient（本地镜头震动）、GamePlatformInputClient（短时输入缓冲）、GamePlatformVFXClient/SFXClient（唯一视觉与音频Provider）。  
MOBA：MobaPresentationRuntime（强度策略）、MobaPresentationClient（可信事件投影/不同Provider请求）。  
项目：DBAClient（英雄技能反馈目录）、DBAArena（竞技可选组装位置）、英雄内容包（真实VFX/SFX/Animation/Profile引用）。普通登录及世界不强制依赖竞技。

## P0 — 基线与冲突防护

- [x] 定位 `Game/DivineBeastsArena.uproject`、根AGENTS及现行46＋N插件基线。
- [x] 确认已有GamePlatformCombat、Presentation、VFX、SFX、输入和Moba事实适配，并保留无关GAS/技能资产修改。
- [x] 明确旧五行、破元、共鸣与项目历史方案已废止；当前GAS属性已精简，不能为本次反馈重新引入权威暴击属性。
- [ ] 每次批量合并前对照Git差异和正在进行的其他构建，避免多任务覆盖。

## P1 — 通用参数及资料契约

- [x] `GamePlatformPresentationCore/Public/Feedback/GamePlatformHitFeedbackProfile.h`：轻/重/技能/格挡、破防附加帧、强度、闪白、连击、相机、SFX、VFX可调参数。
- [x] 增加OverlayMaterial（覆盖材质）和CameraShakeClass（镜头震动类）软引用，由编辑器生成真实Profile资产且由上层预加载。
- [ ] 实际使用Monolith/UE编辑器生成3种参考Profile和十二生肖技能对应资产，标记来源、参数、后备方案、软资产租约。当前编辑器原有未保存UIProfile且新C++类尚未重载，不得私自关闭编辑器丢失资产。

## P2 — 局部视觉顿帧与输入连续性

- [x] `GamePlatformLocalHitstopSubsystem`（本地玩家视觉顿帧）：使用 `FTSTicker::FDelegateHandle`，60Hz参考时长、最大10帧连续窗口、GUID去重、原状态恢复、世界退出清理。
- [x] 保留服务器GAS、World Tick、摄像机、粒子、声音和输入采样，不使用全局TimeDilation。
- [x] `GamePlatformActionInputBuffer`（输入缓冲）：默认最多8事件、250ms有效期、同BindingGeneration和递增Sequence检查。
- [x] `DivineBeastsInputClientSubsystem`（项目输入接线）：暂停结束后只重放当前Pawn已认证输入，继续由GAS合法性与取消窗口判定。
- [ ] RootMotion和客户端/服务器真实CharacterMovement未完全解耦，必须在真实受击、跳跃、预测与网络纠错场景验证后才允许启用完整视觉暂停。

## P3 — 九层表现实际执行

- [x] GamePlatformAnimationClient新增 `GamePlatformHitFlashWorldSubsystem`，以既有Mesh OverlayMaterial实现≤2参考帧目标局部高亮，保存/恢复原材质；无材质时安全跳过。
- [x] GamePlatformCameraClient新增 `GamePlatformCameraHitFeedbackSubsystem`，按LocalPlayer播放已经预加载的CameraShake；支持用户0强度关闭，不震动UI。
- [x] MobaPresentationClient针对同一已确认Hit分发两个独立稳定Layer GUID至原生平台 VFX 和 SFX Provider，0帧顿帧不会阻断其它反馈。
- [x] 保留战斗权威数值与UI只读展示的既有接口，不复写GAS伤害或独立Niagara/SFX播放器。
- [ ] 真正的动画受击、合法的纯表现击退/根运动补偿、伤害数字ViewModel、命中闪白材质、CameraShake曲线、Niagara实例和接触/材质音资源须经引擎制作、Cook与截图复核；本批C++接口不是已完成的视觉资源。

## P4 — MOBA类别、连击与反馈幅度

- [x] `FMobaHitFeedbackPolicy`：轻击/重击/技能/格挡/挥空确定性参数决策；连击强度封顶，不逐次叠加顿帧，格挡/挥空立即减弱。
- [x] 实际战斗 `SourceAbilityId`（服务器技能ID）传递到网络表现事实与MOBA适配，已标识的技能按Skill强度默认处理。
- [x] 记录致死事件与同次Damage事实共用GUID的风险，对Death映射独立派生GUID避免去重互相吞噬。
- [ ] 真正的重攻击、格挡、破防、连击段数还需由服务器技能/状态规则明确输出；不能从伤害大小或客户端猜测。已取消的暴击规则不得当作现有权威玩法恢复。

## P5 — 项目英雄映射和真实内容

- [x] `UDivineBeastsCombatFeedbackCatalog`（项目反馈数据资产类）：HeroDefinitionId＋AbilityDefinitionId唯一键、Profile软引用、VFX/SFX逻辑DefinitionId。
- [x] Moba客户端接受已加载Profile与两个逻辑DefinitionId进行无同步加载的组合调校。
- [ ] 真实英雄技能Profile、正确英雄/技能身份、项目内容包注册和自动预加载/租约尚未交付；要先核对现有技能Definition资产，再为丑牛、寅虎、卯兔首批真实技能创建映射，禁止制造假技能名称或.uasset。
- [ ] DBAArena竞技组合根按真实已授权技能ID接入匹配与生命周期释放；公共DBAClient保持非竞技独立。

## P6 — 网络事实与位移权威

- [x] 在现有 `GamePlatformCombatComponent` 增加服务器单向Unreliable `MulticastConfirmedCombatFeedback`，只传最小命中表现信息，不改变GAS与GameplayCue。
- [x] `UGamePlatformCombatFeedbackWorldSubsystem`（世界事实总线）只读广播给MobaPresentationClient，不采用全局进程单例；传输使用GUID、角色/世界代次进行过期过滤。
- [x] `SourceAbilityId`由权威CombatSpec流入CombatEvent，再到可选网络表现事实和MOBA请求。
- [x] 本轮加强网络迟到事实校验：目标角色代次与世界代次必须严格匹配当前复制状态，既拒绝旧代次也拒绝提前到达的新代次；伤害事实中护盾吸收不得超过本次伤害。新增对应异常数值自动化断言。该变更会在复制状态尚未追上表现事实时安全丢弃可选表现，不改变GAS结算。
- [x] 本轮重复执行静态门禁：`ValidateProjectHeaders.ps1`（599处引用、0缺失）、`ValidateInheritanceBoundaries.ps1`（477个公开头、978个类型、173条继承边）和`git diff --check`均通过。另有并行UI测试文件变更，未予修改。
- [ ] `DivineBeastsArenaClient` 的 `GamePlatformCombat` 定向编译已重新启动，只有实际完成并取得退出码0才可标记构建通过。
- [ ] 双客户端真实确认网络Relevancy、丢包、预测撤销、重复EventId、LateJoin和断线重连；纯表现不承担真实击退或碰撞，真实位移仍由服务器Gameplay决定。
- [ ] 目前只实现客户端已确认事实路径，完整预测/纠正业务和权威位移实现尚未验收。

## P7 — 验证/性能/可视化

- [x] 新增MOBA反馈0/3/6参考帧策略自动化测试源码。
- [x] 新增GamePlatformInput缓冲生命周期自动化测试源码。
- [x] 新增GamePlatformCombat网络事实边界测试源码，以及Camera舒适度倍率测试源码。
- [x] 已执行静态项目头文件与三层Public继承边界检查（本轮最终结果待写入验收日志）。
- [ ] UE Automation真正执行；本地/远端目标销毁、分屏、多PIE、World退出、资源切换验证。
- [ ] 使用真实统计检查5v5命中密度、Niagara和SFX并发、服务端最小Cook和帧耗。不得虚构FPS或内存数字。

## P8 — 构建、项目资产与交付

- [x] 使用锁定 `F:/UnrealEngine-5.8.0-release` 的真实Win64工具链启动Client定向构建，已根据第一轮编译诊断修复Ticker句柄与include拼接。
- [ ] 对新增UHT反射类型完整重新生成，再通过Client/Editor/Server三个目标、至少一个已执行Automation、干净Cook及双客户端人工技能演示。
- [ ] 完成Monolith/UE生成真实DataAsset/Material/CameraShake/Widget资产的编译、保存、重新加载和引用检查。
- [x] 更新本工单、主实施计划、插件文档与总体目录规划；任何未执行的验收项必须保持未完成。
- [ ] 不提交、不推送、不删除其他并行任务未提交更改。

## 2026-10-09 继续实施：P1/P2/P5可运行链增量

- [x] **P1｜数据资产身份修正。** `UGamePlatformHitFeedbackProfile`、`UDivineBeastsCombatFeedbackCatalog`改为继承唯一`UGamePlatformDefinitionBase`主资产基类，复用既有`GamePlatformData`版本、身份与租约；Profile的Overlay/CameraShake软资源仅在`Client` Bundle申请时预加载，平台Profile增加参数区间与非有限值校验。
- [x] **P5｜目录映射与实际组合根。** 目录每行以`FPrimaryAssetId ProfileDefinitionId`作为租约加载的稳定身份，不再使用无法被当前DataService直接租约化的普通DataAsset软路径；新增项目目录重复键、主资产ID和128行边界校验及UE自动化测试源码。此处因暂无正式反馈资产，可无迁移风险地调整未发布字段。
- [x] **P5｜竞技客户端桥。** `DBAArena/DivineBeastsArenaClient/Private/Feedback`增加真实`ULocalPlayerSubsystem`组合根；根据当前已确认命中的SourceActor角色身份、Avatar代次和SourceAbilityId选择已加载Profile，通过MOBA可选中立Resolver返回VFX/SFX逻辑ID。Catalog通过可配置主资产ID异步加载，Profile首次需要时通过`GamePlatformData`的Client Bundle异步请求，最多持有64个Profile租约，无同步磁盘加载。LocalPlayer不是世界Outer，租约采用实例期限并在WorldCleanup/LocalPlayer注销时主动释放；回调核对请求代次，加载失败不因每次命中反复申请。
- [x] **P2｜RootMotion防护。** 平台动画顿帧遇到正在播放真实根运动的Character或被其他系统事先暂停的Mesh时主动跳过；只有实际受理至少一个Mesh才返回成功并登记命中ID。此为未完成RootMotion网络隔离之前的保守策略，不宣称根运动也能冻结。
- [ ] **资产/构建门禁。** `UDivineBeastsArenaCombatFeedbackSettings::CatalogDefinitionId`默认空，必须由实际编辑器创建并通过主资产扫描和Cook后配置；丑牛/寅虎/卯兔真实技能与反馈Profile尚未建立，本轮不伪造.uasset或逻辑身份。资产未加载时仍使用平台默认命中反馈。UE Client/Editor/Server定向编译、自动化实跑和多人评审待实证。
- [x] **本轮静态门禁。** 自有头文件检查614处0缺失；三层Public头477、类型981、继承边175，退出0。仅代表源码边界检查，不代表UE真实编译。
- [x] **正式结构组合门禁。** 首轮`ValidateDesignBaseline.ps1`定位到竞技Client/Editor缺失的两个直接插件描述依赖（GamePlatformCore、GamePlatformCombat，共4条目标错误）；已最小增量补全`DBAArena.uplugin`后重跑，Client/Server/Editor装配均通过，机制插件46与已登记16个内容插件总量未变化。
- [ ] **风险审查。** 异步Data租约和回调需UE实测世界退出、账号切换、分屏、本地玩家删除；第一击发生时对应Profile尚未预加载会使用通用反馈，正式上线前应基于已授权技能/角色出现事件提前预热。对于RootMotion动画，当前主动跳过视觉顿帧而不是篡改服务器位移。

### 本轮资产预热、场景隔离与UE编译证据补充

- 竞技组合根在已确认Catalog租约成功后，按已配置目录条目顺序去重并主动异步预热最多64个Profile（只请求Client Bundle，VFX/SFX内容通过各自平台Provider与目录处理）。同一World不会无限重试失败目录，Profile失败不影响GAS或恢复操作；配置数超过容量的其它条目使用平台通用反馈，不假装全部加载完成。
- 仅真实`AGamePlatformArenaGameState`所在竞技World才请求目录和Profile；OpenWorld、Village、登录/选角或非竞技场景不因为装配`DBAArena`被动加载MOBA纯表现资源。PostLoadMap早于GameState复制时不标记加载尝试，首次真实命中可重检。
- MOBA层已有项目Resolver但未取得当前英雄/技能的Profile时不沿用上一英雄的旧VFX/SFX定义ID；回退通用强度/局部顿帧，不播放错误生肖的特效。
- UE5.8`DivineBeastsArenaClient Win64 Development`定向编译运行：UHT真实通过，处理13个反射生成文件；后续9个C++编译动作进入当前UE5.8分支的UBA本地执行器，`cl.exe`长时间无CPU进展，最终主动停止本次仅我方创建的Build Job，**没有编译成功退出码**。引擎`ExecutorFactory.cs`证实此分支即便使用`-NoUBA`仍走UBA，仅关闭Detour；应先排查本机UBA状态后重跑，不重复宣称切换到了MSVC本地直接执行器。
- 本批编译有部分源码在启动后更新（Resolver fallback、目录预热与ArenaGameState范围保护），所以即便首轮编译有成功记录也不能作为最终变更的全量验收。Client/Editor/Server构建、真实Profile资产、UE自动化和Cook/联机仍未完成。

## 验收证据与回退

优先执行现有 `Tests/Architecture/ValidateProjectHeaders.ps1`（头文件）、`ValidateInheritanceBoundaries.ps1`（三层依赖）、`git diff --check`（差异卫生）。随后使用仓库锁定UE构建脚本，保持真实失败日志。  
对局视觉失败应跳过对应可选层，不修改服务器结果。账号/英雄切换与World释放必须清空局部顿帧、Overlay、输入缓存、Profile引用与Provider请求；网络迟到事实不得恢复旧世界资源。  
仍未能满足“九层游戏内全部可见”“12生肖全部技能配置”“完整P0—P8验收通过”，交付中不得宣称已经完成。
