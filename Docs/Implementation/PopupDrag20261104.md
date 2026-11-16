# SSLM 弹窗基类拖拽实施计划

> 当前会话使用 superpowers:executing-plans 串行实施，完成后进行一次独立代码审查。用户已要求直接修改，并明确「应该修改基类」；不增加确认关卡，不复制工程或切换共享工作区。

**Goal:** 现有交互弹窗通过继承获得鼠标拖动能力，保留按钮、物品格、输入框及原业务行为。

**Architecture:** 修改既有 `UGamePlatformWidgetBase` 与 `UGamePlatformActivatableWidgetBase`。两个基类接收 UMG 原生输入/生命周期事件，委托给 GamePlatformUI 内唯一的实例级拖拽实现。页面资产只声明边界、移动节点与顶部拖拽区，不增加项目播放器、窗口管理器或逐页蓝图逻辑。

**Tech Stack:** UE 5.8 / CommonUI / UMG / Slate，Monolith 原生资产修改，既有 Foundation 增量构建与 Client Cook。

**Spec:** 用户本轮「弹窗应该可以使用鼠标拖拽」「应该修改基类」；下方交互合同。

## 交互合同与实现选择

- 左键按住弹窗顶部标题/木框空白区域拖动；默认顶部 40 逻辑像素，界面资产可配置。正常子控件先处理鼠标，不能在 Preview 事件抢走按钮、滑块、输入框和物品格点击。
- 窗口内容和底板共同移动，遮罩及全屏场景背景固定。通过现有节点的 RenderTranslation 移动，不改文字大小、Canvas 布局和命令绑定。
- 在实际 LocalPlayer 视口内限制移动，正确换算 DPI；小于视口的窗口保持完整可见，过大窗口保留顶部可操作区。
- 仅当前鼠标用户/指针控制捕获。释放左键、丢失捕获、应用失焦、隐藏、失活与销毁取消拖动；只释放自己的捕获。
- 窗口关闭/Slate 销毁恢复默认位置；临时被模态页覆盖保留位置。视口或 DPI 改变时取消拖动并恢复设计位置，避免旧坐标使窗口丢失。
- UI 只通过输入、可见性、自适应和生命周期通知更新，无 Tick / 常驻计时器。
- 现有独立窗口接入，包括背包、系统菜单、设置、错误提示、通用面板、Toast、组队及竞技匹配/确认/结算。全屏任务、图鉴、社交、活动、选人和计分页面保持其全屏布局；它们仍继承相同基类能力，可由定义启用。
- 方案选择：逐页实现会形成多套捕获状态机；运行时重建 WidgetTree 会破坏绑定。采用基类转发 + 唯一内部实现 + 现有 `PopupPanel.json` 配置。

## Global Constraints

- GamePlatformUI 不引用项目资源；项目映射继续由 `Tools/Unreal/UI/PopupPanel.json` 唯一维护。
- 原生资产只通过当前正式工程 Monolith 修改/编译/保存/独立重载；编辑前备份。
- 保留其他任务和前轮修改；不提交或重置共享工作区。当前无用户编辑器/客户端，Village/MainArena 保持运行。
- Cook、预览、测试和临时文件均在工程 `Saved`。禁止 HWebCodex。
- 固定增量参数 `-NoSharedPCH -NoUBA -MaxParallelActions 8`；Editor 额外 `-UsePrecompiled -AdditionalPlugins DBAServer`。
- 运行前核对实际双服务器战斗合同，`t.MaxFPS 0`、`r.VSync 0`；构建/原生/打包/人工验收分别报告。

## Review Focus

- 标题区域有关闭按钮：子控件点击优先，不能误移动窗口。
- DPI 与多窗口：同样的物理鼠标位移产生正确视觉位移，状态不串窗口/指针。
- 遮罩与场景背景：拖动后仍固定，窗口外操作不能穿过模态层。
- 隐藏、失焦、失活、销毁与重显：不残留捕获，不恢复旧拖动。
- 视口缩小或窗口大于可用空间：标题保持可操作，不缩放文字或控件。

## Task 1 — 基类共享实现及原生回归

Files: `Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Source/GamePlatformUIClient/{Public,Private}/Core/GamePlatform*WidgetBase.*`、`Public/Input/GamePlatformUIWindowDragSettings.h`、`Private/Input/GamePlatformUIWindowDrag.*`、`Private/Tests/GamePlatformUIWindowDrag*`。

- [x] 通过真实基类回调记录原生失败，覆盖普通和可激活两类宿主。
- [x] 添加共享配置和内部唯一实现，两个基类只转发输入/生命周期/自适应通知。
- [x] 原生测试验证拖动、DPI、边界、指针隔离、释放、失活/销毁及禁用默认值；执行相邻生命周期/输入回归。

## Task 2 — 既有弹窗资产接入

Files: `Tools/Unreal/UI/PopupPanel.json`、`AuthorPopupPanelUE58.py`、当前 14 个浮动窗口资产及原有生成登记。

- [x] 原生备份与记录全部布局、按钮、字体和父类合同。
- [x] 用同一作者入口配置基类拖拽区域/移动节点，确保背景能命中、遮罩不移动。
- [ ] 编译保存后在独立 Editor 重载，验证配置和布局不变；真实窗口做输入及截图验证。

## Task 3 — 构建、打包、运行

- [x] 完整 Editor / Client 增量构建、相关原生测试与 Client Cook/Stage。
- [x] 核验真实 Village/MainArena 服务器与新版 Client 共享战斗源码、复制类型、构建和 Cook 身份；不因纯客户端变化无故重启服务器。
- [ ] 运行新版客户端供用户人工拖动背包、组队和提示窗口，记录自动验证与人工验收边界。
- [x] 一次独立审查并处理相关问题，交付实际证据。

## 执行记录

- 起点：main `0939b156`，保留前轮 7 个未提交 UI 文件；服务器 PID 58708 / 41700。
- 只读 Editor PID 31652，工程/Monolith 9316 已核实，无脏内容。20 个背景表面中 14 个是浮动窗口，6 个是全屏页面。
- 基线原生检查：两个现有基类均没有 WindowDrag 配置；实际缺少通用拖拽能力。记录 `Saved/Validation/PopupDrag20261104/NativeBaseline.json`。
- Ruling: 保持现有正式工作区，不另建内容副本；用户明确要求在基类直接修正，且原生资产必须在正式工程落地。

- 原生回归：普通/可激活基类初次均因缺失 WindowDrag 配置失败；实现后正常/DPI回归通过，5项相邻生命周期和1项通知回归通过。
- 14 个浮动弹窗通过唯一作者配置、编译、保存；独立 PID 32164 冷重载20页，14启用/6保持全屏。布局、字号、控件身份不变；所有移动节点祖先无旋转或渲染缩放。
- 审查 P2：小视口中背包标题离屏。新增原生回归的首次布局/缩小两断言先失败；加入生命周期/视口事件驱动的一次布局收敛后，三个拖拽测试全部通过。审核者已确认问题关闭。
- Ruling: 用 NativePaint 在子树实际布局完成后消费一次待收敛标记，避免构造时使用无效几何；无 Tick、定时器或业务轮询。窗口过高时仍保持标题可达。
- 完整 Editor `cc84842c-646a-4ee0-ab57-84cf9c66c7d5`、Client `88e88805-8541-439d-82ee-245647a6ab26` 均成功；RHI `604ee65d-f024-46c0-a6b0-8ac307001c99` 已通过。Cook 与实际客户端人工验收仍待本轮后续证据。
- 前期失败保留：测试构造参数编译错误已修正；模块单编译后编辑器启动崩溃，完整增量重建派生模块后独立编辑器启动正常。所有失败记录保留在本任务 Saved 目录。

- 交付门禁：Client Cook/Stage `f4f91d34-763e-46eb-9f9f-3f7b72b7f214` 通过。当前编译输入与打包身份、174份共享战斗/GAS/契约及真实Village/MainArena PID 58708/41700核验通过；19项差异限定客户端UI及生成登记，无战斗代码变化。
- 已启动新客户端 PID 68316/8408，均进入真实登录UI，日志确认 `t.MaxFPS=0` / `r.VSync=0`。Windows锁屏阻止鼠标操作；已请求用户解锁登录。真实世界准入、人工拖动和玩家签收仍未通过，不以启动/离屏回归冒充。详细状态见 `Saved/Validation/PopupDrag20261104/Delivery.json`。
