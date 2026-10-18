# SSLM 67插件质量审查和人工可视化验收计划（2026-10-17）

审查范围：DivineBeastsWorkspace（工作空间）中的 Game/DivineBeastsArena.uproject（虚幻引擎UE5.8主工程）。三层架构保持 GamePlatform（平台）→ MobaCommon（MOBA通用）→ DivineBeasts（SSLM项目）。此文记录实际检查和已落盘修改，不把静态检查、资产包头、编译语法或演示菜单视为正式运行验收。

## 一、全量插件检查基线

从真实 .uplugin（插件描述）、Build.cs（模块构建配置）、C++源码、原生 .uasset/.umap（资源/地图）以及两个正式编目读取：67个插件，包含48个代码插件、19个内容包，声明89个模块。交付编目另外记录8个保留模块。当前67行台账已生成于本目录 CurrentPluginQualityMatrix.json 与 CurrentPluginQualityMatrix.md。

检查物理插件身份、模块规则文件与三层描述依赖，未发现静态结构错误；不等于完成了67个插件所有API、蓝图、资源与运行流程的审查。45个插件拥有模块内C++测试源。GamePlatformLobby（大厅）和GamePlatformVillage（村庄）分别只有3个默认模块入口源码，GamePlatformLocalization（本地化）只有一个默认模块入口源码；这些是已审查的能力不足或保留模块线索，不得把模块注册当作业务功能完成。

## 二、可视化和人工验收的硬性要求

当前按视觉/交互/内容插件分类需逐项人工验收的有53个，其中51个没有正式登记的可操作功能案例。已登记4项功能、9个案例，0项人工签收通过。案例缺失是交付缺口，不代表没有实际开发关卡。当前工程已有42张开发目录 .umap 候选地图及实际插件总览蓝图、场景覆盖UI，但必须逐张核实UE资产引用、实际效果、功能归属、操作、恢复与性能后才可签收。

禁止平行新建第二套演示中心。现有DivineBeastsDemoClient（SSLM演示客户端模块）的插件总览在开发模式具有搜索、插件/功能/案例列表、启动、执行、暂停、继续、停止、重置、返回、证据入口。本次源码已改为稳定ID复用列表对象、中文化实现和接线状态，并为已有实际开发关卡提供编辑器专用DiagnosticMap（诊断地图）菜单：在GamePlatformDeveloperTools（平台开发工具）分组中显示真实AssetRegistry开发地图，经UE包存在检查后可由已有Launch（启动）和Return（返回）操作路由打开。只能标记为诊断，不计正式功能分母、操作适配器和人工验收通过数。

人工操作建议：进入受控开发模式→插件总览→选择 GamePlatformDeveloperTools →选择开发诊断地图→启动→在真实关卡和已有覆盖UI中观察/操作→返回总览。若具体场景缺少可操作控件、测试用参数或证据，该场景仅为待完善的观察地图，不能签发功能完成。对正式案例需使用插件→功能→案例→启动→参数或操作→停止/重置→证据→真人审核全过程。

对于VFX（视觉特效）、SFX（音效）、摄像机、水体、天气、PCG（程序化生成）、UI（用户界面）、角色和材质，应实现真实展示资产、参数调整、性能档位、失败提示和资源回收；不可视服务可以使用可操作的诊断面板，不强制制作空3D场景。每个插件功能必须另配真实Adapter（运行适配器）和测试用例；不允许为达标而只填JSON或新建空蓝图。

## 三、性能与最佳实践审查策略

静态工具自动收集非测试、非编辑器专用C++中的Tick（逐帧执行）、Ticker（定时轮询）、同步加载和全世界Actor遍历候选。经排除Editor-only（仅编辑器模块）与显式WITH_EDITOR（编辑器编译路径），当前主要候选为23处Ticker、22处Tick。它们是需阅读上下文的代码线索，不是45处确定的性能缺陷，未测量实际CPU/GPU耗时。

优先查：只有活跃对象才注册轮询；世界或实例销毁时注销委托/Timer；异步回调校验对象/世界/代次；数据资源必须有独立租约和取消完成；禁止编辑器材质、UI、Niagara纹理被专用服务器硬引用。UI大列表要使用虚拟化、对象复用和事件驱动；Niagara、PCG、天气、水体和角色系统需分别用Unreal Insights（虚幻性能分析）、CPU/GPU Profile（处理器/图形处理器画像）、实例数、DrawCall（绘制调用）和不同画质档实际测量。不自行设定未经批准的FPS、CPU或显存通过阈值。

应按P0-P3闭环：
- P0：插件描述、模块身份、继承与反向依赖阻断；最新静态文件检查未发现P0结构错误，仍需正式UBT验证。
- P1：51个要求正式操作型人工界面但未登记案例的插件；用同一DemoHub逐插件完成真实Widget/蓝图/地图/适配器、操作和回退。
- P2：无本地测试源码或存在热点候选的模块；核对线程、权限、生命周期、网络断线、服务器资产隔离并补有意义的真实测试。
- P3：所有功能均需正式UE5.8编译、蓝图冷重载、Client/Server独立Cook、多人测试、性能记录、真实审核人签收。

## 四、本批确实完成的代码与测试

- 新增 Tests/Architecture/AuditCurrentPluginQuality.py：真正逐一读取67份物理插件的源码和资源并产生完整JSON/中文Markdown台账；缺少真实原生和人工证据时发布检查必须失败关闭。
- 新增 Tests/Architecture/TestCurrentPluginQualityAudit.py：7项审查器单元回归，包括漏编目拒绝、67个身份一致、人工通过始终为0、编辑器同步加载不误判为运行时风险。
- 更新DivineBeastsDemoHubScreen.cpp：搜索列表项复用、中文四维状态、不把开发诊断入口误显示成正式人工通过。
- 更新DivineBeastsDemoHubViewModel.cpp：大小写不敏感搜索、受限开发地图诊断入口、复用原有目录OpenScenario/ReturnToHub；没有把诊断地图写进正式功能登记。
- 新增 Tests/Architecture/TestPluginVisualDiagnosticRouting.py：4项真实原生包头和路由的静态回归。现有开发地图和UI资源真实存在，但蓝图运行/点击行为仍未由本批UE验证。
- 既有插件编目、演示清单和多个PluginDemo静态回归已按真实工程运行通过；该结果只说明合同一致，不能代替真人打开UE及点击验收。

## 五、尚未完成及真实发布门禁

67个插件的完整业务语义/性能实测和蓝图可视化审查仍在推进，尤其51个正式案例缺口未制作完成。没有UE5.8最新全量构建/所有蓝图冷重载/独立Client与Server Cook/Unreal Insights记录/真人签收，均不能声明生产交付。工作空间另有正在使用的编辑器和未提交改动，本批不覆盖或保存已被其他任务加载的.uasset/.umap，不抢占编译与蓝图制作。

使用工作空间根目录中的固定入口重新盘点：

- python Tests/Architecture/AuditCurrentPluginQuality.py --write-docs --summary  （生成真实插件质量台账）
- python Tests/Architecture/AuditCurrentPluginQuality.py --require-production --summary （严格发布阻断检查；未验收时退出2）
- python Tests/Architecture/ValidatePluginDemoManifest.py --manifest Docs/Implementation/PluginDemos/PluginDemoManifest.json --check-docs （正式演示清单一致性）
- python Tests/Architecture/TestCurrentPluginQualityAudit.py （全量台账单元回归）
- python Tests/Architecture/TestPluginVisualDiagnosticRouting.py （真实演示地图路由静态回归）

不能把静态检查通过解释为已完成全部插件性能优化或人工验收。

## 六、2026-10-18 实施进展：真实开发地图入口与人工验收分界

- 从实际存在的UE5.8原生.umap中核验13张不重复诊断地图，利用项目现有DemoHub（插件演示中心）在编辑器模式下为19个插件/内容包提供只读人工观察快捷入口。已关联PCG、VFX寒冰箭、水体、天气、摄像机、战斗、SFX、交互、AI、环境表面、开放世界地形、英雄/动画、竞技及鸟群专题。所有地图资源只检查包头签名和存在性，未声称真实渲染或操作完成。
- VFX寒冰箭审核地图原位于DBAPresentationPack_Core（项目表现内容包）独立挂载点，本批只对明确的`/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review`目录开启Editor-only（仅编辑器）AssetRegistry发现并限制为`L_DBA_IceArrow_A_Review`单张地图；禁止将内容包其他地图或正式业务地图误作为开发演示功能。
- 域内快捷入口增加启用状态约束，非GamePlatformDeveloperTools（公共诊断入口）域必须处于已启用状态；复用原Catalog.OpenScenario与ReturnToHub路由，没有伪造功能注册、适配器、人工记录或发布批准。
- DemoHubScreen（演示页面）按FName稳定身份缓存菜单项，改为TMap索引与取用删除；避免搜索和刷新时跨插件/功能/案例复用同一个值对象及O(n²)列表扫描，复用行才执行必要的视图刷新。仅源码逻辑优化，尚无真实设备耗时基准，不能量化性能提升。
- 全插件审核脚本增加`diagnosticMapPackage`（可供观察的原生地图），但正式`registeredScenarios`（已注册操作案例）保持不变；新台账明确19个插件对应13张真实开发诊断地图，仍是53个视觉/交互领域、51个未登记正式操作案例、4功能9案例、人工签收0。
- 当前静态验收：`TestPluginVisualDiagnosticRouting.py` 10项通过；`TestCurrentPluginQualityAudit.py` 8项通过；旧地图路由6项通过；`TestPluginUIReadiness.py` 5项通过；ModuleGraph（模块依赖）2项和Composition（目标装配）17项通过；Public API继承/类型边界扫描571份头文件、1238类型和210关系通过。改动的演示中心3个C++编译单元在锁定UE5.8/MSVC下通过仅语法验证`/Zs`，不是正式UBT链接或UHT/蓝图运行。
- 未完成：Editor存在其他活动进程，尚未对13张地图做可见窗口人工点击、蓝图冷加载、性能实测、UI回退和真人签收；不可把13张诊断图计成51项正式演示案例的已完成工作。后续优先针对VFX、PCG、摄像机、天气、水体、战斗和交互将诊断图升级为实际功能适配器，并独立保留运行/人工证据。

## 七、2026-10-18 运行期专项新增结果

- GamePlatformSession（跨服）：修复同步失败成功误报、取消后仍启动旅行及事实回调自身生命周期。Debug/Release纯C++内核真实各1/1项通过；UE传输3个C++单元完成MSVC语法检查。新的UE原生联机测试尚未运行。
- GamePlatformVFX（视觉特效）：实例化鸟群按物种5～60Hz固定Tick间隔，原生测试源码补动态更新率；两份C++语法检查通过。统一环境群落代码合同和BirdFlock发布规则分别通过。
- GamePlatformTelemetry（遥测）：修正连续重试时历史一次性Ticker句柄累积、重入Shutdown重复预算、调度失败未收口风险；增加2个原生测试源码，两份C++语法检查及架构门禁通过，未运行最新版Automation。
- 本批恢复插件安全静态回归10项通过、Session内核Debug/Release各1项、鸟群发布43项与合成Cook3项、模块图2项/目标装配17项/服务端生命周期5项通过。完整证据与必须继续验证的功能见 `Docs/Implementation/PluginQualityAudit/RuntimeSafetyReview_20261018.md`。
- 仍维持67插件/89模块基线与19个诊断快捷入口/13张真实地图；51个要求人工操作的正式演示案例缺口尚未解决，人工签收0。严格发布门禁不得通过。


