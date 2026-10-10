# StylingAndContent（样式与内容）

## 通用主题机制（2026-10-10）

`UGamePlatformUIThemeDefinition`（平台主题定义）继承平台统一Definition，Buttons/Texts/Borders（按钮/文字/边框）软引用CommonUI原生样式类。项目只创建本类型数据实例；纯外观差异不增加空的C++派生类。

`UI.Style.*`中立语义先做PlatformId/SkinId/QualityTier条件过滤，再比较精确语义、来源作用域、具体度、显式Priority。同级歧义拒绝；父级回退由调用方显式允许。项目选定主题是完整有效候选集合，不自动注册未激活内容包。

原有UIManager按LocalPlayer拥有私有ThemeService，资源由GamePlatformData提供实例租约。新主题完整加载/校验前保留旧主题，成功才广播OnThemeChanged；取消、退出和迟到结果不得覆盖新请求。不存在全局可变CDO、独立StreamableManager或逐帧主题查询。

两种平台Widget基类新增ThemeBindings（命名控件→语义），默认空保持历史蓝图行为。绑定器兼容原生UButton/UTextBlock/UBorder及CommonUI同类；只读复制画刷/字体，不接管业务命令。文本默认保留字号；CommonText清除当前实例旧Style后应用快照，CommonBorder设置新Style，避免后续原生同步恢复旧视觉。必需绑定错误时不部分更新当前页面。

## 资产边界

平台Content/Client/Defaults（中性默认）、Templates（模板）和Development（开发）不含具体游戏美术身份。项目公共主题与样式归第三层真实内容包DBAUIPack_Core。英雄图标与可选皮肤仍各自拥有，不被公共主题硬引用成完整角色资源包。

主题Definition声明NeedsLoadForServer=false，但真正Server Cook剥离仍需产物核查。正式发行排除Development资源。所有项目主题/样式/WBP必须由Monolith在真实UE编辑器创建、编译、保存、重载，普通文件工具禁止伪造.uasset。

实施计划与未通过的运行/资产门禁见工作空间Docs/superpowers/plans/2026-10-10-ui-theme-and-shared-styles.md及Docs/Implementation/UIThemeImplementation_20261010.md。单文件编译不等于完整模块、蓝图和Cook通过。

