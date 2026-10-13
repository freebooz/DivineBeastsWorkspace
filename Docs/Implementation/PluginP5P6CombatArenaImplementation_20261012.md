# 《SSLM》P5–P6 战斗、AI、交互与竞技演示实施计划（2026-10-12）

工程：DivineBeastsWorkspace（SSLM工作空间），UE5.8；Git基线：ecb9d85617947b83996dafd8914fab15e8c252e2；状态：已完成阶段性实施与专项验证；真实可玩战斗/AI交互及生产竞技闭环仍阻塞。详见同目录PluginP5P6ExecutionResult_20261012.md。

## 一、范围与不可越过的权限门禁

本轮实施的是第二批P5–P6，不重做第一批P0–P4，不覆盖其他会话已修改的12生肖Definition、前端UI和Village地图。不新建多套GameMode/Combat/Arena实现，开发期场景只使用既有正式类，源码改动在既有插件对应模块。

- P5：GamePlatformCharacter（角色）、GamePlatformAbilitySystem（GAS）、GamePlatformCombat（战斗）、GamePlatformAI（AI）、GamePlatformNavigation（导航）、GamePlatformInteraction（交互）、GamePlatformQuest（任务）和DBAGameplay（项目玩法）。
- P6：GamePlatformArena（通用竞技）、MobaPresentation（竞技表现）和DBAArena（项目竞技）。
- 实际服务器权威不可借助设置bIsServer或客户端本地Damage模拟替代；角色资格、可信匹配分配、准入Token及奖励全部须走正式服务。
- 真实测试地图必须由UE5.8编辑器创建.umap，UI资产修改只通过Monolith；本轮不修改现有UI资产，也不把开发夹具烘焙进正式Shipping。
- 生产Arena项目模式目前显式NotConfigured：必须保留FailClosed，不为做演示把其改成配置完成或使用FoundationTest MapId假冒真实比赛。若后端可信分配和正式项目规则缺失，则只能交付开发场景及资格门禁测试，真实竞技闭环保持阻塞。

## 二、实施顺序及验收

### P5-0 源码和依赖审查
- [x] 读取工程规则、三层边界、已有ArenaGameMode、AIController、项目PlayableCharacter以及角色出生逻辑。
- [x] 发现生产Arena五模式配置未发布；当前公共世界和Arena默认Pawn出生均有服务器门禁，不能绕过。
- [x] 已核对AIController必须依赖可信Definition/Brain、Interactor服务端RPC校验、Editor可用反射类及既有开发地图排除规则；AI/Interaction模块在旧编辑器尚未加载。

### P5-1 原生行为与权限回归
- [ ] 制作独立权威门禁测试，覆盖不可由无有效玩家身份的临时Pawn激活GAS伤害、非法输入、旧生命周期代次拒绝、客户端不能操作服务器权威。
- [ ] 基于现有能力测试执行客户端/服务端目标的官方自动化，记录实际通过/失败/未运行。
- [x] 在现有GAS/Combat/GameplayEligibility/ArenaGameMode源码中保持原权限链；新增6项静态门禁已通过，真实官方自动化合计48/48通过（部分Warnings），未改写生产准入。

### P5-2 开发场景与资源
建议新增且不得覆盖现有资源：
- `/Game/Development/Foundation/PluginReviews/Maps/L_GP_CombatReview`（角色、训练沙盒、技能/战斗权限审查）。使用现有角色类及共享Manny/Quinn原型资源，不新建重复玩法计算器；角色出生与真实技能激活需对应正式可信注入才开放。
- `/Game/Development/Foundation/PluginReviews/Maps/L_GP_InteractionAIReview`（AI/Nav/Interact/Quest）。演示环境、NavMeshBoundsVolume、官方AI控制器/互动组件作为审核夹具；缺可信AI Definition不能伪造运行状态。
- `/Game/Development/Foundation/PluginReviews/Maps/L_DBA_MainArenaReview`（正式通用ArenaGameMode的非Shipping等待分配/状态复制场景），不改生产MainArena规则、也不把样例MapId当正式分配。
- [x] 三图使用真实UE5.8 Editor创建，3/3独立进程回读通过；两张项目角色/开发GameMode蓝图已真实保存，项目角色绑定公共DBA SkeletalMesh后独立重启回读通过。AI/Interact/Quest行为对象仍缺模块反射和正式资源，只能标为场景布局及可信准入审查，不是完整可玩演示。

### P5-3 单客户端与专用服务器实际运行
- [ ] 已尝试Editor完整/定向编译，均受-NoEngineChanges引擎模块产物保护阻断；Client编译触发900+动作后为避免共享引擎并发冲突受控停止，Server新目标和Cook未验证；三图资源回读3/3已通过。
- [ ] 尝试单客户端进入CombatReview/InteractionAIReview和专服世界，采集GameMode、Actor、NavMesh、Pawn、技能拒绝/许可和WorldExit日志；不以视觉布景代替技能命中成功。
- [ ] AI实际寻路、交互完成及任务状态需要合法配置资产，缺失时记录阻断。

### P6-1 竞技五模式的真实资格测试
- [x] 官方GamePlatform.Arena（竞技插件）8/8通过；项目DBAArena11/11通过（真实已注册子鼠Hero参数），覆盖1v1—5v5定义与项目未配置禁入合同。
- [x] 静态门禁6项以及通用Arena8项中生产NotConfigured、Roster/Ticket拒绝、请求权限等合同验证通过；还不是生产后端真实签票联机。
- [x] 真正UE地图通过独立回读确认使用AGamePlatformArenaGameMode，默认Pawn为null、两队各5个真实PlayerStart；实际运行状态WaitingAssignment尚待专服回归。
- [ ] 完成断线宽限、重连、计时器释放、Result待提交/确认的现有原生自动化用例。

### P6-2 真实对局与闭环（依赖后端和产品配置）
- [ ] 绑定经批准的5种ModeSpec、MainArena MapId、内容Revision、胜负/超时/出生策略、实际选人规则。
- [ ] Go业务后端发起可验证比赛分配并完成身份票据签发。
- [ ] 一次真实Dedicated Server + 2客户端1v1经过准入→选人→倒计时→出生→战斗→结算→提交→安全返回；其余四种人数模式至少容量及准入/客户端同步回归。
- [ ] 只有这个闭环、Client/Server Cook与目标设备实测均通过后才能把P6标记完成。

## 三、工程方法与证据

任何已有map、DataAsset或模块应先检查，再增量修复；脚本作者必须显式工程/引擎/路径验证、不覆盖目标资产、禁止生产参数被调试值修改；新地图默认位于Development并由发布Cook门禁排除。

每步在 `Saved/Validation/PluginP5P6_20261012` 输出输入SHA、编译、UEAsset回读、自动化和运行记录。测试报告中的 Success/Failed/NotRun 分别统计；手工视觉签名独立保留。结束时更新本文及单独实际结果，明确已完成/未完成/阻塞和恢复条件。
