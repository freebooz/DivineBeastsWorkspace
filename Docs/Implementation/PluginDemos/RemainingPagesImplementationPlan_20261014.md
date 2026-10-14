# SSLM剩余界面原生接线与蓝图执行计划

> **For agentic workers:** REQUIRED SUB-SKILL: 使用 superpowers:executing-plans，Native 逐项执行；不分派实现工作，不覆盖并行任务。

**Goal:** 补齐目录中11份页面/HUD/通知蓝图、12项材质审核页及演示中心详情、控制、报告三页，形成真实事件驱动显示和命令交互。

**Architecture:** 公共界面继续由DBAClient/DivineBeastsUIClient提供原生类型，DBAUIPack_Core拥有资产；竞技界面归DBAArena并只消费MOBA复制投影。演示三页共享Hub拥有的唯一ViewModel，页内不另启BeginPage、不创建第二执行器；材质审核保留既有兼容父类及12项样板合同。

**Tech Stack:** 锁定UE5.8.0、Monolith MCP 0.23.0、UMG/CommonUI、现有平台UI/任务服务与项目只读ViewModel。

**Spec:** `PluginDemoCenterSpecification.md`、`PluginDemoCenterImplementationPlan.md`、`../Materials/MaterialLibraryUIWidgetSpec_V1.json`及本轮用户明确列出的15份资产范围。

## Global Constraints

- 依赖方向为DivineBeasts→MobaCommon→GamePlatform，非竞技公共界面不得依赖竞技。
- 所有UI资产创建、布局、样式、编译、保存和结构复核仅经Monolith；不使用已知错误的widget截图模式。
- 先完成代码和蓝图，再统一正式Editor目标编译与运行验证；不构建Client/Server，不Cook，不自动人工签收。
- 直接修改当前main已获授权；不创建工作树、不提交、不重置，不修改水体并行任务。
- 不伪造进度、服务名单、英雄许可、命令成功、报告或人工通过。缺失服务明确失败关闭。
- 控件不使用业务Tick；选择使用稳定身份，委托按页面/本地玩家生命周期绑定解绑。

## Review Focus

1. 页面失活、换ViewModel及世界退出后，旧回调不能恢复旧内容或发起命令。
2. 同名任务/公告与列表刷新不按展示文本猜身份，不将程序性选择当用户命令。
3. 后端未提供英雄选择/训练操作时，禁用动作并显示原因，不以UI补造权威合同。
4. 演示三页不能各自BeginPage或抢占同一样板；参数非法、案例切换、忙碌及证据失败应有真实反馈。
5. 共享编辑器有并行脏包时，不保存他人包或强制重启；最终构建证据必须与当前源码匹配。

## 文件责任与任务

### Task 1: 公共11项中的9项

**Files:** 修改Boot、Quest、Social、LiveOps、WorldHUDBase及NotificationWidget原生类型；新增相应Private/Tests回归；资产只新增目录中缺失路径。

**Interfaces:** 消费既有LoadingSnapshot、QuestClient缓存、Social/LiveOps只读投影、UIViewModel命令和World投影；输出命名控件合同及实际状态显示。

- [ ] 写资产/真实显示回归，记录修改前缺失页；用户要求统一编译，新增C++测试在最终阶段运行，不宣称已观察原生RED。
- [ ] Boot显示真实阶段及未知/已知进度；Quest提供稳定ID选择、详情及本地追踪；Social提供好友浏览及已有小队入口；LiveOps浏览真实公告；四HUD提供世界/角色/目标提示及手册/训练命令；Toast消费实际通知请求。
- [ ] Monolith创建Screens/{Boot,Quest,Social,LiveOps}、HUD/{OpenWorldHUD,VillageMainHUD,TutorialGuidance,TrainingControls}及Notifications/Toast，保存并回读。

### Task 2: 竞技英雄选择与记分板

**Files:** DBAArena的ArenaScreenBase及Private/Tests；新增两个既定DBAArena UI资产。

**Interfaces:** 消费GamePlatformArenaViewModel复制名单/英雄/准备/比分；输出可读阵容与记分板。当前没有授权英雄候选与选人命令合同，不将持久角色列表冒充赛前许可。

- [ ] 写真实复制投影显示测试及空服务拒绝测试，最终统一运行。
- [ ] 完善名单、英雄、准备及比分呈现，尚无选人能力时明确禁用确认/准备并显示缺口；关闭只影响本页。
- [ ] Monolith创建HeroSelection、Scoreboard页面，设置真实行蓝图及初始焦点。

### Task 3: 演示详情、控制、报告三页

**Files:** 兼容DemoHubWidget增加共享模型投影桥；HubScreen只绑定子页；新增Private/Tests；仅定点调整既有Hub布局。

**Interfaces:** 消费HubViewModel.GetMenuSnapshot/RequestOperation/RequestEvidence；三页为HubWidget派生的可复用子页，订阅而不拥有模型页面生命周期。

- [ ] 写失活/重绑/参数及报告不签收测试，最终统一运行。
- [ ] Detail显示当前功能依据/步骤/前提/缺口；Controls使用类型参数行与真实能力命令；Report显示运行身份、操作/清理/终态及实际证据索引，人工保持未核验。
- [ ] Monolith创建三页并嵌入Hub的页签切换区，移除被替代的旧内联显示，保留插件/功能/案例列表和筛选。

### Task 4: 材质库审核界面

**Files:** 使用既有12项规格及平台材质函数/审核母版约定，不触碰水体资产；更新审核页规格的SSLM文案与自适应布局。

**Interfaces:** 12张真实Image→专用MD_UI材质→平台函数；ReturnButton复用已有返回服务。

- [ ] 核对真实函数、母版和已有同名资产；缺失依赖不能仅给Image染色冒充材质效果。
- [ ] 通过Monolith创建审核母版与界面，保留83项命名合同；滚动/换行布局不整体缩小文字。
- [ ] 对每项Brush、函数调用与颜色/透明度连接回读留证，人工结论不自动填入。

### Task 5: 集中验证与交付记录

**Files:** 更新执行进度、资产生成清单、目录规划和测试说明，仅增量编辑本人范围。

- [ ] 检查15页树/父类/命名/导航/保存；若编辑器有他人脏包，继续独立工作，不关闭编辑器。
- [ ] 所有实现结束后，执行既有BuildFoundation正式Editor入口，安全重启并独立重载资产，运行新增及受影响自动化。
- [ ] 对实际失败定位整改；保留未执行项、并行基线漂移和联机/Cook/设备/人工验收边界。
- [ ] 进行一次独立最终只读评审，修复重要问题，不自动提交或签收。

## 执行台账

- 2026-10-14 Pre-flight：HEAD b70c28f4dfae20ad1b9b84b80497457a0b2506f3；存在本人上批UI改动及并行Water改动，全部保留；正式UE5.8编辑器PID11536，Monolith0.23.0，非PIE。
- Ruling: 本轮为已批准方案的明确续做，用户明确不要问询；不重复发起规格/工作树确认。若理解有误的成本为需调整本轮局部界面，不触及业务权威。
- Ruling: 用户要求代码/蓝图先完成、统一编译；测试先写，原生RED/GREEN与完整Editor证据留到最后，不把缺少新DLL或未加载测试记为通过。成本为最终阶段可能集中暴露编译错误。
- Ruling: 不自动提交或清理工作目录；共享main存在并行工作，保留可追溯的文档与原始证据，不用提交混入他人修改。
- 结构回归：Monolith patch采用完整声明树语义，局部规格会移除未列出的旧控件。已保存错误后现场；修复采用变更前完整dump并显式替换详情/动作子树，恢复三组原列表及其EntryClass，不使用整体重建。
