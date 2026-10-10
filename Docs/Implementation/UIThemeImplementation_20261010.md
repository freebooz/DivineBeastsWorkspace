# 跨项目UI主题与神兽联盟统一样式实施记录

计划：Docs/superpowers/plans/2026-10-10-ui-theme-and-shared-styles.md（T0—T6）。
本次用户授权为主题设计落地，不续做之前生肖技能任务；保持固定工作空间，保留其他Weather/PCG修改，不主动提交、推送或重置。

## 一、已落盘的机制

平台GamePlatformUIClient新增：
- Public/Styling/GamePlatformUIThemeTypes.h：上下文、类型、来源排序、必需语义、命名控件样式绑定。
- Public/Definitions/GamePlatformUIThemeDefinition.h及Private/Definitions/GamePlatformUIThemeDefinition.cpp：继承GamePlatformDefinitionBase，Buttons/Texts/Borders（按钮/文字/边框）软引用CommonUI原生样式；资格过滤、精确语义、来源优先级、具体度、显式Priority和同级歧义拒绝，显式父级回退。
- Private/Styling/GamePlatformUIThemeRequestState.h与GamePlatformUIThemeService.h/.cpp：原UIManager内部的LocalPlayer主题服务。当前与在途各一份GamePlatformData租约，失败保持旧主题，GUID/租约代次拒绝迟到回调，关闭只释放己方需求，不新建Subsystem或StreamableManager。
- Public/Styling/GamePlatformUIThemeBinding.h及Private/Styling/GamePlatformUIThemeBinding.cpp：先预检命名控件和类型，再应用原生样式；不重新创建树、改业务命令或改共享CDO。
- Private/Tests/GamePlatformUIThemeTests.cpp：DeterministicResolution（确定性解析）、DefinitionValidation（字段校验）、RequestCancellation（取消资格）三个原生自动化测试。

原有UIManager新增RequestThemeAsync/CancelThemeRequest/GetCurrentThemeId/GetThemeRevision及OnThemeChanged/OnThemeRequestFinished，服务由该管理器初始化和退出。两种平台控件基类增加ThemeBindings，默认空数组保持旧蓝图行为；在构造/激活末尾应用，销毁/失活注销委托，避免样式回调关闭页面后继续绑定。

MobaCommon不新增主题系统，仅保留既有竞技语义。第三层DivineBeastsUIClientSubsystem从GGameIni的[DivineBeasts.UI.Theme]读取DefaultThemeDefinitionId，交给平台请求一次，失败只记中文诊断，不阻断登录、不自动认证、不逐帧重试。

## 二、兼容与资源规则

UButton仅替换Normal/Hovered/Pressed/Disabled画刷，保留当前padding、点击声音、命令、焦点及业务禁用状态；UCommonButtonBase走原生SetStyle。UTextBlock复制字体和颜色，默认保留已验收字号；UCommonTextBlock先清除当前实例旧Style再应用只读快照，防止后续SynchronizeProperties把字号和颜色恢复。UBorder复制主题画刷；UCommonBorder同时记录新Style类，防止重建时恢复旧背景。

隐藏页的绑定对象保留已应用样式类强引用，原主题租约撤销不会产生悬空绘制引用。当前主题状态属于LocalPlayer，不存在进程级可变主题或CDO写入。主题Definition声明NeedsLoadForServer=false，主题资源仍须用最终Server Cook清单验证排除。

Scope表示项目显式选定主题内部的来源，当前版本不开放任意未激活内容包自动注册；动态目录片段需以后独立加入真实包激活/撤销与权限验证。

## 三、项目资产与配置状态

Game/Config/DefaultGame.ini已将/DBAUIPack_Core/UI/Themes加入既有GamePlatformDefinition主资产扫描，并增加[DivineBeasts.UI.Theme]节。DefaultThemeDefinitionId目前保持空，不能在真实主题资产不存在时启用。

推荐正式主题：/DBAUIPack_Core/UI/Themes/DA_DBA_UITheme_Default，逻辑ID为GamePlatformDefinition:dba.ui.theme.default@1。按钮、文字和边框样式应通过Monolith创建在UI/Styles。现有FrontEndClient配置已明确烘焙整个公共UI目录，不向全局Server增加AlwaysCook。

当前本记录创建时还没有生成真实主题或样式.uasset，Monolith此前提示编辑器未运行，必须在新DLL实际加载后生成并补记。旧登录LOGO、背景、输入框、控件树与密码处理均未修改。

## 四、真实检查与限制

- 测试先行：初始源合同检查缺少7项实现而失败；实现后8项通过。随后增加CommonUI重新同步回归，2项按预期失败，修正后扩展的14项源码合同全部通过。入口Tests/Architecture/ValidateUIThemeIntegration.py；它不是UE运行证明。
- 锁定F:/UnrealEngine-5.8.0-release：主题测试源码经UHT及单文件C++编译通过（退出0）；七个实现文件ThemeDefinition、ThemeService、ThemeBinding、UIManager、WidgetBase、ActivatableWidgetBase、项目UIClientSubsystem均分别UBT -SingleFile成功。之后的重入/原生同步/服务器排除修订需最终再编译。
- 完整Editor模块构建指定GamePlatformUIClient和DivineBeastsUIClient，实际依赖图138项，在900秒上限超时，最后输出8/138动作；没有新的完整UI DLL链接成功证据。不能用SingleFile通过冒充全模块或实际主题切换成功。
- 已尝试启动锁定正式编辑器；具体新启动和Monolith结果在后续记录追加。所有视觉资源仍坚持Monolith生成，未伪造.uasset或资产清单。

## 2026-10-10 最新执行顺序与代码续补

用户追加要求“先代码及蓝图，最后统一自动化”。本轮不重复运行上述历史测试，先完成T5资产。已修正显式主题绑定的原生UButton背景乘色：设置统一样式后将BackgroundColor恢复白色，避免旧页面的局部蓝色/深色与新主题Brush相乘导致项目主题不一致；保留内容色、字号、尺寸、焦点、命令和禁用状态。同时仅编写对应源码断言，留到最后集中运行。现有引擎构建为其他会话已有工作，未主动终止或重复发起。

## 五、未完成门禁与恢复顺序

完成最终C++和完整DLL链接→启动主工程并确认ThemeDefinition/ThemeBindings反射→运行GamePlatform.UI.Theme.*原生测试→Monolith创建五种原生样式和默认主题/中性测试主题→绑定实际命名控件、编译保存重载→启用默认主题ID→两套主题/LocalPlayer隔离/取消回退实测→客户端Cook与服务器剥离审计。

当前可证明主题基础机制、项目接线已写入且主要源文件完成定向编译；真实主题外观、默认启用、全模块运行与Cook不能宣称完成。T0—T6验收状态应按独立证据更新，而非仅按文件数量。
