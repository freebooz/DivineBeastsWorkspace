# 《神兽联盟》双开发启动模式及插件门户 P0–P5 执行记录

基线：DivineBeastsWorkspace／UE5.8／main `ecb9d85617947b83996dafd8914fab15e8c252e2`。
目标：`Debug（完整流程调试）`从原有 `L_DBA_FrontEnd` 登录开始；`PluginDev（插件开发）`不进行登录，进入统一门户，再选择插件演示并返回。仅为非Shipping且非Dedicated Server提供。

## 真实工程基线
- `Game/Config/DefaultEngine.ini` 的 `GameDefaultMap` 是 `/DBAFrontEndPack/Maps/L_DBA_FrontEnd`。正式入口保持不变。
- `Game/Source/DivineBeastsArena/Private/DBAGameInstance.cpp` 只有本实例基础协调器，不能建立第二套认证流程。
- `DBAClient/DivineBeastsUIClient/Private/Adapters/Application/DivineBeastsApplicationUIAdapter.cpp` 首个本地玩家初始化时默认 `StartFlow(false)`，是必须隔离的登录入口。
- `GamePlatformDeveloperTools` 的 `UGamePlatformReviewSubsystem` 为EditorOnly，不可直接承担客户端运行导航。
- `/Game/Development/Foundation/PluginReviews/Cases` 已有65份ReviewCase；已有鸟群、天气、雪、PCG、战斗、AI、SFX、英雄与竞技等UE审核关卡。
- 当前UI工程约束必须用Monolith MCP修改或创建Widget Blueprint；本对话HWebCodex的本地MCP列表未返回可用Monolith服务。门户视觉资产不得用普通脚本/伪造二进制代替。

## 分步实施

### P0 启动模式及安全协议
1. 在项目UI客户端模块中增加单一解析器：`-DBALaunchMode=Debug`（默认）和 `-DBALaunchMode=PluginDev`。
2. 仅非Shipping、非命令行验证、非Dedicated Server可进入PluginDev；与`FoundationStandalone`等冲突则失败关闭至Debug。
3. 为纯解析器增加原生自动化，覆盖非法、冲突和端侧。
4. 不在 `.uproject`、`DefaultEngine.ini` 或生产Target中持久修改默认地图。

### P1 客户端项目UI分流
1. 修改已有UI Adapter和UI Subsystem，在PluginDev阻止应用流程 `StartFlow(false)` 与登录/角色页面自动路由。
2. 不伪造Authenticated；不直接触发后台HTTP、Session或后端准入。Debug分支维持现有逻辑。
3. 保留已存在RootLayout和平台界面服务，不复制第二套UI总线。

### P2 演示门户目录与关卡
1. 在DBAClient的UI模块新增 `UDBAPluginDevelopmentCatalog（演示目录数据资产）`、展示条目和合法性校验。
2. 真实UE编辑器创建 `/Game/Development/Foundation/PluginReviews/Maps/L_DBA_PluginDevPortal` 及目录 `DA_DBA_PluginDevCatalog`，登记真实插件身份、ReviewCase与软地图引用；可运行关卡与逻辑测试明确区分。
3. `DBAUIPack_Core` 的 `WBP_DBA_UI_PluginDevPortal` 由Monolith创建、编译、保存和独立加载。若服务未连接，则标记为阻塞，不用Engine Python/普通工具代做UMG。

### P3 单实例开发演示与返回
1. 在DBAClient新增 `UGameInstanceSubsystem（实例级子系统）` 保存目录软引用和当前演示身份。
2. UI按钮调用 `OpenDemo/ReturnToPortal`，只允许目录中合法存在的`/Game/Development`地图；未授权、未知插件、未制作的地图失败返回并保留门户。
3. 常规 `OpenLevel（切换地图）` 创建/释放世界，不构造服务器有效登录身份；过期世界、网络世界和Shipping均拒绝。
4. 不为65个插件创建65个地图：纯逻辑显示“自动化审核”，有场景才启用“进入演示”。

### P4 开发启动器
1. `Build/Game/LaunchDBADevelopment.ps1` 提供 `-Mode Debug|PluginDev`，解析正式主工程、真实引擎路径及地图，避免硬编码个人磁盘目录。
2. 调试模式不自动登录，保留手工认证；PluginDev明确传开发参数和门户地图。
3. 运行器只允许Development客户端/编辑器，不能当作生产客户端交付入口。

### P5 验收
1. 纯启动解析器 +目录验证C++自动化；
2. Editor/Client/Server目标编译与端侧隔离；开发地图及目录数据资产由真实UE独立重新加载；
3. Debug真实登录和PluginDev无登录门户验证；至少五类映射（VFX、SFX、PCG、Combat、Hero）实际进出、连续10次返回与异常地图处理；
4. 正式Cook/Server不包含纯开发门户和开发审核地图；
5. 人工界面、焦点/键盘/鼠标、画面及声音验收必须真人签字，自动化不可代签。

## 安全边界
工作区有其他未提交修改，禁止覆盖已有UI蓝图、角色场景、DefaultGame.ini和业务控制面。新增UE资源用引擎创建；UMG资产只通过Monolith。
本记录中的阶段结果必须依据本轮编译、运行、Cook和独立资源回读逐项填写，不能仅凭代码和资产文件存在认定完成。

## 执行状态（随真实结果更新）
- P0：待执行
- P1：待执行
- P2：目录/地图待执行；Monolith门户Widget待服务连接
- P3：待执行
- P4：待执行
- P5：待执行；人工验收需审核人
