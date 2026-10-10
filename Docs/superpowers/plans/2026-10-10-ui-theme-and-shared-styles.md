# 神兽联盟界面主题与统一样式实施计划

> 执行方式：按本计划在当前会话逐项增量实施；每项先测试后实现。用户已明确要求“生成执行计划并按计划实施”，不再重复请求同一任务授权。

**目标：** 不改页面业务逻辑，实现跨项目不同背景/按钮外观、项目内相同语义一致样式，保留现有UI命名控件、焦点、尺寸与输入绑定。
**架构：** GamePlatformUIClient（平台客户端界面模块）提供主题定义、确定性解析、UIManager拥有的主题服务与兼容绑定；MobaCommon（可选竞技层）只保留通用竞技语义；DBAClient/DivineBeastsUIClient（项目客户端UI模块）选择主题，DBAUIPack_Core（第三层公共UI内容包）拥有真实CommonUI样式与项目主题资源。复用GamePlatformData（平台数据服务）定义/资源租约，不新增插件或并行资产管理器。
**技术：** 锁定UE5.8、C++、CommonUI（通用界面框架）、UMG（界面控件）、DataAsset（定义资产）、Monolith MCP（引擎内资产工具）。
**设计依据：** 本会话已提交的“平台统一样式能力、项目数据决定外观、页面只选语义与组件”方案，以及根AGENTS.md、GamePlatformUI/Docs/StylingAndContent.md、插件开发规范P13、项目用户界面设计。

## 全局约束

- 固定DivineBeastsWorkspace工作空间、Game/DivineBeastsArena.uproject主工程、Game/Plugins唯一源码根；不切换工程、不覆盖未提交变更、不自动git提交或推送。
- 当前正式基线为47个机制插件（含新增GamePlatformWeather），项目代码仍仅五个DBA插件。主题不得另建插件。纯内容包归第三层内部，不是第四层。
- 不修改既有登录LOGO/背景、表单宽高、字号、密码处理或角色选择业务；原UButton/UBorder/UTextBlock先兼容，不强制重建蓝图父类与命名绑定。
- 主题及CommonUI样式只在ClientOnly模块或客户端内容内；Server Cook排除纯表现。不得把Blueprint/C++编译成功当成Cook与联机通过。
- 所有资源加载走GamePlatformData；没有LoadSynchronous、自建StreamableManager或全局可变主题CDO。主题状态属于LocalPlayer；每次切换完整成功后才替换旧主题。
- 语义键使用FName且限定UI.Style.*命名空间，不需要动态创建GameplayTag。上下文只允许PlatformId/SkinId/QualityTier明确等值约束；解析遵循资格过滤、精确语义优先、ContentPack>Project>Moba>Platform、等值约束具体度、显式优先级，同键同优先级歧义拒绝。
- 纯视觉差异使用UGamePlatformUIThemeDefinition实例；神兽联盟没有新结构时不增加空的项目派生主题类。
- 所有项目主题、样式、控件蓝图由Monolith在核对后的正式UE5.8编辑器生成、编译、保存和回读；连接/构建失败则保留阻断，不写伪.uasset。
- 界面事件驱动，无主题Tick轮询；当前控件销毁或失活时注销回调，新页面/重新激活时使用当前主题。隐藏页保留其已应用样式强引用，避免释放切换前主题后悬空。

## 审查重点

1. 迟到加载结果、取消、连续切换与LocalPlayer退出不得覆盖最新主题或泄露租约。
2. 两个LocalPlayer与Multi-PIE（编辑器多实例）不得通过共享CDO相互污染。
3. 缺失/重复/错误类型样式必须在替换当前主题前拒绝；同等候选不能靠数组顺序取第一个。
4. 既有命名UButton等必须保留类型、事件、尺寸/字体可选保留及登录敏感输入清理逻辑。
5. 正式打包必须验证主资产发现与软资源闭包；主题机制交付和美术/蓝图/运行证据单独记录。

## T0：基线、目录及方案落盘

- [x] 读取当前根规范、主题相关源码、GamePlatformData现行接口、CommonUI锁定引擎公开API，检查工作树/并行任务。
- [x] 明确最小可用主题支持Button/Text/Border（三类样式），复合面板用Border背景＋独立标题/内容组合；输入框与其余高级控件后续沿同一契约扩展，不复制整个界面。
- [ ] 同步Docs/README与总体目录规划，建立实施记录。

## T1：主题定义、上下文规则与确定性解析

文件：GamePlatformUIClient/Public/Styling/GamePlatformUIThemeTypes.h、Public/Definitions/GamePlatformUIThemeDefinition.h、Private/Definitions/GamePlatformUIThemeDefinition.cpp、Private/Tests/GamePlatformUIThemeTests.cpp。

接口：FGamePlatformUIThemeContext（平台/皮肤/画质上下文）、FGamePlatformUIStyleRule（语义与覆盖约束）、UGamePlatformUIThemeDefinition（复用LogicalId/DataVersion，持有typed Buttons/Texts/Borders软样式类数组）。TryResolveStyle返回唯一匹配索引或明确错误，不进行资源加载。

- [ ] 先编写用例：精确语义胜过父语义、不同scope排序、错误上下文排除、同级歧义拒绝、父级回退必须显式允许、空与越界输入拒绝。
- [ ] 在未实现定义时记录测试失败或编译缺失类型；补齐最小实现并复测。
- [ ] 验证定义重复键、失效版本、错误类型和必需样式；不修改Shared身份或平台通用数据管理器。

## T2：UIManager所属主题事务服务

文件：GamePlatformUIClient/Private/Styling/GamePlatformUIThemeService.h/.cpp；增量修改Public/Manager/GamePlatformUIManagerSubsystem.h与Private/Manager/GamePlatformUIManagerSubsystem.cpp。

接口：UIManager.RequestThemeAsync(FPrimaryAssetId, FGamePlatformUIThemeContext)、CancelThemeRequest(FGuid)、GetCurrentThemeId、GetThemeRevision、ResolveThemeStyle；OnThemeChanged与OnThemeRequestFinished提供事件。服务只在已有UIManager初始化/退出时创建/清理。

- [ ] 先测试取消/过期/退出的请求资格；资源租约必须完整成功且当前请求仍匹配才提交。
- [ ] 通过AcquireDefinition加载UI分组，验证有效样式类与确定性解析；失败保持旧主题，取消只释放本请求。
- [ ] 替换前校验，替换后通知，终态至多一次；禁止共享CDO写入与全局静态主题。

## T3：兼容绑定与原有基类事件接入

文件：GamePlatformUIClient/Public/Styling/GamePlatformUIThemeBinding.h、Private/Styling/GamePlatformUIThemeBinding.cpp；增量修改Core/GamePlatformWidgetBase与Core/GamePlatformActivatableWidgetBase头/实现。

接口：FGamePlatformUIWidgetStyleBinding记录WidgetName/StyleId/StyleKind/显式回退/字号保留。基类ThemeBindings默认空，旧资产行为不变。Binding只应用同一LocalPlayer当前快照，支持UButton/UTextBlock/UBorder及CommonUI对应组件。

- [ ] 先验证缺失命名控件/不匹配类型不部分改画面，原生控件委托与业务状态不变；更新样式不改共享默认对象。
- [ ] 绑定ThemeChanged，构造/激活立即刷新，退出注销；复制画刷/字体而非改变资源CDO。
- [ ] 需要时持有已应用样式类强引用，让隐藏页面继续安全显示旧样式，重新激活更新。

## T4：项目默认主题接入及发布配置

文件：DBAClient/DivineBeastsUIClient/Private/DivineBeastsUIClientSubsystem.cpp及其Public头；Game/Config/DefaultGame.ini与Custom/FrontEndClient/DefaultGame.ini按实际资产目录更新。

- [ ] 从项目配置读取默认ThemeDefinitionId；未配置时保持旧页面，不反向硬编码DBA路径到平台。
- [ ] 在真实LocalPlayer就绪时请求一次；异步失败可诊断且不阻断登录，无自动业务重试或HTTP。
- [ ] 公共主题资产仅归/DBAUIPack_Core/UI/Themes，公共样式归UI/Styles，默认布局不因主题替换重建。

## T5：Monolith真实主题/样式样板与现有页面渐进迁移

- [ ] 确认编辑器项目与Monolith状态；如未运行按既有入口启动锁定UE5.8。
- [ ] 经Monolith创建BP_DBA_ButtonStyle_Primary/Secondary、BP_DBA_TextStyle_Body/Title、BP_DBA_BorderStyle_Panel（按钮/文本/面板样式），以及DA_DBA_UITheme_Default（默认主题）；深青灰背景、青铜强调、文字优先，使用现有合法资产不生成整屏占位图。
- [ ] 创建中性测试主题或样板用于跨项目验证；测试资源归Development且不进入正式Cook。
- [ ] 保持原UButton类型，在确认的项目页面通过ThemeBindings配置实际命名控件；保存、编译、回读和检查焦点/布局。
- [ ] 更新内容包MonolithGenerationManifest.json真实证据，不把只有C++定义当成真实资产交付。

## T6：编译、自动化、性能与交付

- [ ] 运行静态分层、主题回归、头文件/命名检查与git diff --check。
- [ ] 通过锁定UE5.8编译新增及修改的C++，区分SingleFile、完整DLL链接和编辑器实际加载。
- [ ] 执行GamePlatform.UI.Theme.*原生测试，覆盖同级冲突、跨上下文、取消/连续切换和兼容适配；能启动时在真实编辑器验证两套主题与LocalPlayer隔离。
- [ ] 客户端Cook/Stage资源清单及Server剥离审计在具备构建后独立执行；没有日志不得标记通过。
- [ ] 文档列出已修改、已执行、失败原因、尚缺证据与恢复入口。当前任务不自动提交/推送。

## 实施记录与变更裁决

当前用户明确授权在固定工作空间实施，沿用当前真实工程和Monolith，不另建第二份插件工程。计划以可独立验收的T1—T4机制为首批；T5/T6依赖真实UE新模块加载，无法通过时如实保留而不降低验收标准。最终状态更新在Docs/Implementation/UIThemeImplementation_20261010.md，不使用无证据的“全部完成”。
