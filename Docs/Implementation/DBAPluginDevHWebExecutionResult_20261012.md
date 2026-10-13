# 神兽联盟双开发模式 HWebCodex 分步实施验收记录（2026-10-12）

**项目工作区**：`E:\\work\\2026\\DivineBeastsWorkspace`（用户本轮明确要求 HWebCodex；不得与 Cwebcodex 的另一工作区互相替代）。  
**任务范围**：P0—P5；Debug（完整流程调试）从既有前端登录，PluginDev（插件开发）跳过登录直接打开开发门户并跳转独立场景。  
**保护条件**：保留正式 `DefaultEngine.ini` 默认前端登录、原有GameInstance/PlatformFlow/服务器架构及其他并发未提交内容。不使用非Monolith工具创建Widget蓝图。

## 实际进度与证据

| 阶段 | 现状 | 实际证据 | 未完成工作 |
|---|---|---|---|
| P0 启动模式 | 开发入口源码已存在 | `FDivineBeastsPluginDemoMode（演示模式策略）` + `FDBADevelopmentLaunchMode（开发启动解析器）`。本轮执行`TestDBAPluginDevDualMode.py`，10/10成功 | 两份历史解析器含不同参数格式，后续应仅维护统一`-DBAPluginDemo/-DBAPluginDemoMap`，避免重复状态；尚未实测生产Shipping |
| P1 登录隔离 | 代码入口已存在 | `DivineBeastsApplicationUIAdapter（应用UI适配）`和`DivineBeastsUIClientSubsystem（本地UI子系统）`识别演示模式且不启动普通登录；旧P0客户端UI模块构建Succeeded | 当前并发UI代码变更后完整客户端/编辑器重新编译、真实启动不登录需复测 |
| P2 开发门户地图 | **本轮真实制作及独立重载通过** | `/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub`由UE5.8 `AuthorDBAPluginDemoHubMap.py`创建，实际21,961字节，6个Actor；另一独立Editor进程运行`ValidateDBAPluginDemoHubMap.py`，退出0，`DBA_PORTAL_MAP_INDEPENDENT_RELOAD_OK 6` | Monolith Widget `/Game/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub`仍不存在 |
| P3 插件目录和导航 | 代码已有，非可玩验收 | `DivineBeastsDemoClient（项目演示客户端模块）`已有`UDivineBeastsDemoCatalogSubsystem（目录子系统）`、`UDivineBeastsDemoWorldSubsystem（场景子系统）`、`UDivineBeastsDemoHubWidget（UI父类）`，支持真实插件枚举、开发地图发现、`OpenScenario/ReturnToHub`。当前用户要求继续复用，不新增第二套同义框架 | Widget由Monolith真实创建编译；5类场景及连续10次世界进退验证；导航菜单交互 |
| P4 两模式启动器 | 源码及语法验证成功 | `Build/Game/LaunchDBADevelopment.ps1`复用`RunPluginDemo.ps1`，Debug为默认并保持登录、PluginDev需显式`-Start`；本轮两个文件PowerShell 7语法检查均0错误 | 真实启动完成/超时/返回码及用户登录流程待验证 |
| P5 编译、Cook、人工验收 | **未全部完成** | 先前Client UI定向编译退出0；本轮 HWeb真实门户地图作者/回读退出0，10项静态回归0失败。Editor一度`-NoEngineChanges`受共享引擎产物保护阻断 | 全新Editor/Client/Server编译，原生用例、客户端门户交互、端侧资源隔离Cook、目标设备性能、人工签署 |

## Monolith UI阻塞

工程`AGENTS.md`规定项目Widget Blueprint（用户界面蓝图）及样式必须通过Monolith MCP操作。**本轮HWebCodex`mcp_tool list`结果为`servers: []`**，即未暴露可用Monolith服务。因此没有改用普通Python/文本生成Widget，也没有把仅有C++父类误报成实际可见门户界面。

在恢复真实Monolith连接后，应创建/回读三项开发专属Widget：
1. `WBP_DBA_PluginDemoHub（插件演示门户）`；
2. `WBP_DBA_PluginDemoOverlay（演示返回覆盖层）`；
3. `WBP_DBA_PluginDemoBirdFlock（鸟群演示控制面板）`。

所有资产保持`/Game/Development/PluginDemos`的正式发行Cook排除范围。Monolith创建后必须验证父类、控件树、事件绑定、输入焦点、退出清理和当前编辑器身份。

## 下一步可执行顺序

1. 排除并发引擎构建占用，更新Editor目标、Client目标相关模块和资源装配后进行真实UE Editor/Client单元及场景测试；不绕过`NoEngineChanges`保护。
2. 获取HWebCodex实际可用Monolith连接，制作真实Widget及目录显示组件，否则P2不能签收。
3. 运行两个模式：Debug必须显示登录并保持人工认证；PluginDev必须直接显示门户且无登录HTTP请求。检查未知插件/非法路径/缺地图失败关闭。
4. 实测VFX、SFX、PCG、战斗、生肖英雄最小五类场景，连续导航进入/返回至少10次，核对世界对象、输入焦点、音频/粒子清理和运行记录。
5. 实际Client/Server Cook与容器文件检查确认开发地图/UI不进入Shipping，收集目标设备和真人审核证据；未签收前保持`pending`。

**本轮结论：技术框架与一张真实门户地图已落地；Monolith可视门户、实际交互和完整交付尚未完成。**
