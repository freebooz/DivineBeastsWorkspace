# 插件演示中心实际实施进度

> 2026-10-12｜开始时HEAD：bad8db3a63c7a24f2fee8257387153d41c376b0e｜工作区：DivineBeastsWorkspace（神兽联盟工作空间）。
> 以下仅记录**真实创建的文件和已运行检查**，未做的Editor编译、地图/视觉验收不得标为通过。

## P0 插件审查（进行中）

- 已读取真实.uplugin（插件描述）和Git跟踪关卡，登记47个代码插件、17个内容包、31张开发地图。
- 已生成 PluginDemoManifest.json（插件目录快照）、FeatureCoverage.md（能力线索）、AssetReuseMap.md（候选地图复用说明）。
- 已扫描508个Public头文件中的592个UFUNCTION（蓝图反射调用）候选，生成PublicApiCandidates.json与PublicApiAudit.md；这是源码可见性线索，**不是592项已完成功能**。
- 待完成：按真实功能语义拆分FeatureId（功能身份），审核具体蓝图及资源在UE AssetRegistry的有效性，逐项确认地图能运行。

## P1 演示模式与登录隔离（源码已实现，编译/运行待验证）

- 新增 FDivineBeastsPluginDemoMode（演示启动策略），要求显式-DBAPluginDemo与精确总览地图参数；Test/Shipping/专服/Commandlet不允许旁路。
- 项目应用流程在演示模式跳过正式Online/Session适配初始化，StartFlow有防御性拒绝。
- 项目UI子系统在演示模式跳过正式根布局和业务页面，应用UI适配器不会触发首次StartFlow(false)。
- 新增Build/Game/RunPluginDemo.ps1（开发启动脚本），地图不存在就拒绝；增加PluginDemoClient定制Cook配置和正式DefaultGame.ini中演示目录排除规则。
- 已通过：插件描述JSON校验、模块配置白名单、目录数目、PowerShell启动脚本语法（PS5读取UTF-8临时BOM副本的解析测试）、已跟踪Git差异格式检查。
- 未通过/未运行：真实Editor/Client编译和不经登录的游戏实例启动；原生单测编译运行；Client/Server Cook与发行资源审计。

## P2 开发演示目录（基础代码已落地，待编译验证）

- 在既有DBAClient内加入DivineBeastsDemoClient（开发客户端模块），由.uplugin白名单限开发目标，不创建第48个代码插件。
- 新增 UDivineBeastsDemoCatalogSubsystem（演示目录子系统），从UE真实IPluginManager（插件管理器）读取发现插件，从AssetRegistry（资产注册表）读取开发地图，保留插件是否启用、地图待验证和诊断地图标签。
- 新增 UDivineBeastsDemoHubWidget（演示总览C++蓝图父类），提供插件列表、场景列表、选择场景与返回总览的接口，不在C++伪造用户界面资产。
- 当前实现属于**演示发现/地图路由基础**；尚未实现各插件的详细参数控制、统一数据资产Definition、完整资源租约和场景返回覆盖层，不能宣布P2完成。

## P3—P8 剩余部分

- P3：真实 L_DBA_PluginDemoHub.umap（演示总览地图）与Monolith创建WBP资产、鸟群/天气完整可交互演示均**未制作**；Monolith已注册但当前UE Editor未运行。
- P4—P7：五类通用模板、逐插件演示案例及17内容包资产画廊尚待实现。
- P8：正在执行UE5.8 Editor开发构建：Cwebcodex作业 wc_job_T0gTkSIfDe1dr9ju；截至本次记录状态为运行中，不代表编译成功。自动化实跑、客户端/服务器Cook及人工视觉签署未执行。

## 变更与风险控制

- 未修改正式默认GameDefaultMap（登录前端）；不清理其他任务未提交修改，不自动提交/推送。
- 实际编译通过后再确认真实UE Editor和Monolith身份、制作地图及UI，不能将P0清单或UFUNCTION数量当成验收证明。
