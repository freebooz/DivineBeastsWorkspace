# Plugin scene and font implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为需要人工核验的插件功能提供对应场景，并将插件编辑器及运行时自有 UI 统一为 10 号。
**Architecture:** 复用现有唯一 Manifest/Catalog/适配器、插件独立页及场景导航；复用当前主题和字体属性。全部原生资产通过正式工程的 Monolith 修改。
**Tech Stack:** UE5.8 C++/Slate/UMG、Monolith、Python、PowerShell。
**Spec:** `Docs/Implementation/PluginDemos/PluginSceneFontDesign_20261030.md`，用户已批准继续实施。

## Global Constraints

- 10 指 `FSlateFontInfo.Size = 10`，包括标题、输入、按钮及运行时 HUD；不缩放控件替代改字号。
- 不新增插件、播放器、业务权威或第二套路由；UI 继续事件通知。
- 保留现有工作树及用户编辑；真实包先备份，脏包不覆盖。工程内保存所有临时、证据及 Cook/Stage 输出。
- 增量构建保留 Win64/Development、`-NoSharedPCH -NoUBA -MaxParallelActions 8`，Editor 用 `-UsePrecompiled -AdditionalPlugins DBAServer`。
- 调试不限帧；构建、资产、Cook/Stage、运行和人工验收分别记录。

## Review Focus

- 输入框有效字体位于嵌套 TextStyle，不能仅改无效旧字段。
- 主题重应用、CommonTextBlock/RichText、动态生成控件不得恢复旧字号。
- 场景导航切图必须保持插件/功能身份；停止失败不能丢弃原运行。
- Editor 夹具的精确 Hub 资格不能因新增场景而意外失效或扩大到正式地图。
- 原生包重载、父类缺失或用户脏包冲突必须真实失败，不能跳过后报全覆盖。

### Task 1: 当前字体合同和动态 UI

**Files:** 修改 `DivineBeastsCameraReviewWidget.cpp`、`DivineBeastsAbilityBarPanel.cpp`、`GamePlatformDebugClient.cpp`、`Tests/Assets/ValidateFrontEndInputFonts.py`、`PluginDemoUIMonolithRecipes.json`。新增工程内字体合同、原生作者/验证工具及必要行为测试。
**Interfaces:** 读取同一 10 号合同；原生工具输出逐资产/控件/字段的旧值、新值、编译保存和重载结果。

- [x] 先原生读取当前活动 Widget/主题，形成真实字号不符合清单；原生验证拒绝非10字体、不完整覆盖或无独立重载，观察修改前不符合及修改后通过。
- [x] 修改动态字号及本地 Slate 样式，更新当前作者与验证合同；历史报告保持原样。
- [x] 运行相关离线回归和完整 Editor 增量构建；再由 Monolith 修改并保存真实 UI，独立重载审查。

### Task 2: 效果场景与界面对应

**Files:** 修改 `PluginDemoManifest.json` 及其派生文档、现有场景导航/资格（只在确有必要时）、所属原生开发关卡及对应 UI。
**Interfaces:** 保留现有 feature/scenario/adapter/review 身份；场景包路径仍从 `map_asset` 派生。

- [x] 原生加载当前 Hub 和各领域地图候选；记录实际 Actor、对象、观察镜头及支持操作，确定每个视听案例的场景关系。
- [x] 复用合格场景；缺失时经 Monolith 补最小专用关卡与真实测试对象。Editor 夹具继续精确宿主，不扩大正式地图资格。
- [ ] 更新唯一作者源、派生 Catalog 和对应页面显示；运行现有 Manifest/路由回归以及错场景、取消、返回的必要回归。

### Task 3: 原生运行与交付

**Files:** 更新审查/进度记录，证据位于 `Saved/Validation/PluginSceneFontAudit/20261030`。
**Interfaces:** 消费前两任务的当前源码、原生资产及关系，不从文件存在推断运行通过。

- [x] 完整增量构建并执行受影响 UE 自动化；资源变化需要时执行相应 Cook/Stage，输出留在工程内。
- [ ] 核对 70 页对应关系、活动界面有效字号 10，验证进场景、执行、停止/取消和返回，记录原生截图与必要视频/音频。
- [x] 一次 fresh-context 总体代码审查；修复实质问题，明确人工签收/GPU/联机未执行项，不自动提交或推送。

执行裁定：用户批准设计并要求继续实施；沿用当前正式工程原位 Native 修改，保留已存在 main 工作树和 17 项修改，不创建第二工程。初始只读资产审查使用已存在 Editor，禁自动编译及 Live Coding；最终源码验证必须使用本次完整增量构建。

2026-10-30 实施进度：Monolith 连接正式 UE5.8，作者 PID 27040；160 个原生 WidgetBlueprint 的 1881 个有效字体字段已回读为10，157个WBP实际修改保存，3个共享CommonTextStyle同步；已新建4个真实观察关卡并原生读取9个领域候选。发现 Water/Maps 同名空图，改用有23个Actor的 Water/L_GP_WaterReview。81案例路由更新并派生真实Catalog；新增6条场景路由回归先失败后全部通过。Editor专属夹具继续精确Hub，未扩大WorldHost/世界定义资格。雪足案例仍依赖真实角色及已启用的雪接触配置，地图设置不冒充其功能或人工已通过。

2026-10-31 复核与验证：独立审查两项Important均已处理：PCG.inspection保留精确Hub宿主，7条场景路由回归完成RED→GREEN；菜单只改插件自有条目/子菜单，借用引擎菜单保持自身StyleSet。最终80案例重新对应9个目标场景；其他纯诊断/动态样板继续复用真实Hub。另发现3个主工程插件桥接WBP，补改58字段；最终163个WBP共1939字体字段为10，PID43468独立重开及目录SHA回读通过，3个共享文字样式均10，输入RenderScale仍为1；4项指定UE UI回归通过。Editor、Client完整增量构建通过，11地图PluginDemoClient Cook/Stage退出0，全部输出在工程内。Cook仅改写2份FileOpenOrder生成日志使启动证明摘要过期，正在按原参数补完整增量证明；未放宽门禁或改变源码。

2026-10-31 交付检查点：完整Editor证明 c6b58b8b 已刷新，严格启动门禁通过；PID46020实际显示VFX独立页及L_VFXReview目标路径。4个新增关卡均有独立重载后的原生截图。完整交互链、窗口/DPI及人工视听仍待续验，未勾选为通过；用户追加新手村水下与游泳/潜水需求，转入该需求的现状核验。逐项交付及边界见 PluginSceneFontDelivery_20261031.md。
