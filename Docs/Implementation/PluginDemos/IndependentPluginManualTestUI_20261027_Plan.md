# 逐插件独立人工测试界面实施计划

> 执行方式：当前会话连续实施，使用superpowers:executing-plans；按用户“确认设计并直接实施（包含必要构建）”授权执行，不重复增加计划审批，不提交或推送。

**Goal:** 给全部70个实际插件制作可单独打开的独立UMG人工测试页，并为已实现功能接入真实操作、状态和清理。

**Architecture:** 复用DBAClient现有目录、ViewModel、页面、ScenarioAdapter和证据链；每页锁定PluginId，借用既有详情/控制/报告子页。按既有单向三层依赖注册领域适配器，内容预览和Editor专有操作分别由现有宿主管理。

**Tech Stack:** UE5.8、CommonUI/UMG、直接Monolith MCP、现有平台公开C++接口、JSON Schema及原生自动化。

**Spec:** `IndependentPluginManualTestUI_20261027_Design.md`及原`PluginDemoCenterSpecification.md`的非冲突合同。新设计确认已明确允许必要构建，取代旧阶段“不构建”限制。

## Global Constraints

- 真实范围为48个自研代码、20个内容、2个第三方编辑器插件，身份来自.uplugin和正式内容登记；插件数量不代替功能覆盖率。
- 仅首次快照加事件通知；页面失活、重绑、取消与World退出清理旧订阅、参数、请求和自身租约。
- 用户手动操作与自动化结果分开；受理、执行、清理和人工签收分开；缺实现/适配/资源必须具体报错。
- 不新增第二套播放器、资产管理器、业务权威、工程或服务器角色；不得让公共客户端硬依赖竞技或Editor/Server专有模块。
- 全部UI资产只能由直接Monolith制作、编译、保存、独立重载；不使用已知重复Gamma的widget截图路径。
- 烘焙、中间文件、Stage、Package及证据全部在当前工程内；不关闭其他任务编辑器，不改账号数据，不提交或推送。

## Review Focus

1. 固定插件页被搜索/选择或迟到回调切到其他插件：T02检查不允许跨PluginId选择/执行。
2. 隐藏/换页仍持旧模型或重复订阅：T02检查页面代次、重绑、清理失败及迟到结果。
3. 空模块、缺适配、缺服务被计成已完成：T01/T03—T06检查真实能力/宿主条件及失败结果。
4. Editor、MOBA或权威服务器代码泄入公共客户端：T06/T08检查模块边界、条件注册和实际目标。
5. UI资产身份与作者/运行定义错配：T07核对70个PluginId、类路径、模型类型、真实控件合同和冷重载。

## T01 作者清单与现有范围

Files: `Docs/Implementation/PluginDemos/PluginDemoManifest.{json,schema.json}`、`Tests/Architecture/ValidatePluginDemoManifest.py`及现有清单测试；作者工具`Tools/Unreal/PluginDemos/AuthorPluginDemoCatalog.py`；`DivineBeastsDemoCatalogTypes.h`及定义/验证实现。

Interfaces: 插件条目增量声明`test_page_asset`派生到客户端定义；领域盘点输入为本工程Saved/Validation/PluginManualUI20261027下三份Inventory.json（17/24/29插件，共286个能力/审阅分组），人工核读后归一到现有唯一manifest。

- [ ] 编写并运行失败回归：实际70身份必须全登记，单插件页路径不可重复/越界，第三方不得构造第四架构层。
- [ ] 核对候选功能的真实Public接口/调用方及状态，给每项当前已实现能力明确正常、失败、取消/恢复和清理等适用案例；空模块/未实现能力不伪造可执行案例。
- [ ] 同步Schema、解析/验证、作者派生及文档生成；运行现有清单与合并完整性回归，保留旧4功能/9案例身份。

## T02 独立页面上下文与入口

Files: `DivineBeastsDemoHubViewModel.{h,cpp}`、`DivineBeastsDemoHubScreen.{h,cpp}`、`DivineBeastsDemoMenuTypes.h`、`DivineBeastsDemoWorldUI.cpp`、目录/导航实现及`DivineBeastsPluginDemoMode.*`；新增相邻`Private/Tests/DivineBeastsIndependentPluginPageTests.cpp`。

Interfaces: 在现有VM增加固定PluginId上下文与页面打开命令；Screen提供EditDefaultsOnly固定插件属性供70个蓝图默认值使用；从中心与受控`DBAPluginTest=<PluginId>`直接入口经过同一目录、Data与UIManager流程，继续要求既有显式DBAPluginDemo资格。

- [ ] 原生回归覆盖固定插件过滤/选择、非法ID、失活、换VM、迟到回调、切换清理失败和直接入口资格；编译前不声称原生RED已运行。
- [ ] 实现单本地玩家活动页与每World唯一运行所有权；固定页仍使用共享子页及同一VM，不在Native生成控件树。
- [ ] 更新第三方发现来源、保持原架构层合同；禁用插件仍可查看范围/缺口，但不能执行其能力。

## T03 纯内容包人工样板

Files: 新增既有DemoClient/Private/Scenarios下的内容包适配与相邻Tests；必要的内容编目/参数合同仅扩展现有链。

Interfaces: 仍实现`IDivineBeastsDemoScenarioAdapter::Prepare/Execute/Stop`，每World独立工厂；通过正式Data租约和现有角色/表现/VFX/SFX服务加载与呈现，不另造资源池。

- [ ] 以20个真实内容包为分母，为各包已有模型/动画/图标/材质/声音/世界资源提供人工选择和预览；身份使用实际Package/PrimaryAssetId。
- [ ] 覆盖无资源、取消加载、切资源、停止、世界退出及注册撤销；未授权V6玩法不因图标预览被授予。
- [ ] 所有预览有真实状态/错误；原生资源与实际视听验收分开记录。

## T04 平台基础与服务功能

Files: DemoClient/Private/Scenarios下服务领域适配和相邻Tests；模块依赖仅主流程统一修改。

Interfaces: 按PlatformServices.Inventory.json的17插件/59分组接入已有公开接口；上下文沿用FDivineBeastsDemoScenarioContext，完成回调原样携回RunToken。

- [ ] 为已实现的Core/Data/Flow/Input/Loading/Save/Settings/Online/Session/Server/Inventory/Equipment/Progression/Entitlement/LiveOps/Commerce能力增加真实操作与类型化参数。
- [ ] 网络/存储前置失败、取消不等于事务撤销、失效结果和清理分别验证；只写本工程测试目录和开发/沙箱端点，密码不进入快照/日志。
- [ ] Localization空模块明确无实现；不在Demo层实现缺失业务。

## T05 平台玩法与世界功能

Files: DemoClient/Private/Scenarios下玩法/表现/世界领域适配和相邻Tests。

Interfaces: 按PlatformWorld.Inventory.json的24插件/76分组，调用真实Gameplay/Character/GAS/Combat/Animation/AI/Quest/Presentation/VFX/SFX/Camera/UI/Surface/World/Weather/Water/PCG/Navigation/Interaction/Diagnostics公开接口。编辑器动作只放Editor条件私有桥。

- [ ] 对实际已实现能力提供参数、真实样板、操作和状态，不把鸟群项目组合当平台唯一测试。
- [ ] 覆盖命中/目标失效、取消、重复事件、停止、恢复、缺资产、遮挡、世界退出等对应边界；不绕过服务器权威。
- [ ] Lobby/Village空模块和不存在的公开能力集授予/撤销入口明确登记，不能编造按钮成功。

## T06 项目、MOBA与第三方功能

Files: DBAClient既有Demo私有项目适配、DBAArena组合根可选适配、Editor条件桥及各自Tests。

Interfaces: 以ProjectContent.Inventory.json中9个代码/第三方插件的70能力组为核读依据；MOBA通过既有组合根注册Factory，第三方调用实际GlooPrint/BlueprintAutoLayout编辑器入口。

- [ ] 提供已有应用/UI/英雄/世界/竞技/服务器适配的人工路径，复用真实业务命令与开发夹具。
- [ ] 编辑器对象选择、宿主不匹配、插件禁用、MOBA关闭、对局清理与错误输入分别验证；未实现竞技规则不得显示已实现。
- [ ] 保留公共DBAClient不硬依赖MOBA/ServerOnly，Client不链接Editor。

## T07 Monolith原生独立页制作

Files: 新增`Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py`及对应只读核验器；复用既有Monolith recipes与DBAUIPack_Core开发目录/GenerationManifest。

Interfaces: 输入唯一manifest的70个PluginId和页面路径；输出70份真实`WBP_DBA_PluginTest_<PluginId>`，复用现有Hub UIScreenDefinition合同，由既有WorldUI生成本次页面的瞬态定义，复用Native Screen、共享子页/参数行和玉石主题。

实施裁定：设计要求每插件独立可寻址UMG，并复用既有页面定义合同；T02已通过瞬态定义替换固定Widget类。取消原计划70份无人消费的持久化定义，避免额外资产和第二套身份登记；不改变70独立页、固定插件、唯一Data/UIManager路由及真实功能接线的验收要求。

- [ ] 先核对Editor工程、模块版本、Monolith UI schema和目标包脏状态；备份已有目标，不覆盖其他脏包。
- [ ] 制作70个可操作页面并接入中心/直接入口，固定插件上下文、功能菜单、参数、样板、诊断与报告。
- [ ] 编译、保存、独立冷重载；逐页核对PluginId、父类、命名控件、焦点、导航、软引用与清单数量，无文本假资产。

## T08 完整构建和实际验收

Files: 复用Build/Game既有构建、原生测试和Cook入口；证据只放本工程Saved/Validation/PluginManualUI20261027。

- [ ] 对最终稳定源码做必要完整目标增量构建：UE5.8 Win64 Development，NoSharedPCH/NoUBA/MaxParallelActions8；Editor UsePrecompiled与AdditionalPlugins DBAServer，Client/Server不用UsePrecompiled。
- [ ] 运行新增必要原生回归和现有菜单/目录/生命周期回归；用真实客户端/PIE打开全部70页并验证键盘、鼠标、切页、关闭、错误和恢复。
- [ ] 若发布需要Cook/Stage，全部目录在工程内，验证客户端开发资产合同和专服剥离；真实联网前绑定实际Village/MainArena与Client版本。
- [ ] 每功能记录真实运行及待人工结论。全部已实现功能接线和页面可运行前不宣称全覆盖；用户人工签收由用户完成。
- [ ] 完成一次独立全改动审阅并修复实质问题；交付实际结果、入口、证据与尚需人工操作项。
