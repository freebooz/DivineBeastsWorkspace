# 神兽联盟主题接入规则

平台GamePlatformUIClient提供主题定义、解析、生命周期与兼容控件；MobaCommon只保留竞技语义。现有DivineBeastsUIClientSubsystem选择项目主题，真实皮肤与CommonUI样式归DBAUIPack_Core/UI/Themes、UI/Styles，不创建独立主题插件。

## 配置

项目Game/Config/DefaultGame.ini包含[DivineBeasts.UI.Theme]的DefaultThemeDefinitionId，目前为空。只有Monolith生成并验证实际主题后才填写GamePlatformDefinition:dba.ui.theme.default@1。未配置时保留原页面；失败不阻断登录、不发送HTTP或触发自动认证。

## 迁移

通过Monolith向真实Widget的ThemeBindings字段配置WidgetName、StyleId、Kind、bAllowParentFallback、bPreserveFontSize。名字必须来自当前Widget树，不猜测登录或背包按钮名。主要按钮语义UI.Style.Button.Primary，次要UI.Style.Button.Secondary，正文UI.Style.Text.Body，标题UI.Style.Text.Title，面板UI.Style.Panel.Default。平台只提供机制，项目数据选择具体颜色、字体与纹理。

默认空绑定不改变历史UButton类型、原有点击/焦点和密码字段；无需为了换皮肤重建业务类或全部蓝图。原生文本默认保留字号。CommonUI文本使用当前控件实例的只读样式快照以避免原Style重复覆盖字号；公共类CDO永不修改。

## 验收

源合同Tests/Architecture/ValidateUIThemeIntegration.py；原生测试前缀GamePlatform.UI.Theme。先验证一套真实主题和样板页，再逐个领域接入。所有蓝图/主题/样式由Monolith创建、编译、保存、重载和记录；具体实现进度见Docs/Implementation/UIThemeImplementation_20261010.md，不能由本说明推导实际运行或发布通过。

## 2026-10-11 自然远古定稿

最终改为自然、藤蔓、树叶、碧绿、原始与远古，替代此前商代青铜。第三层DBAUIPack_Core拥有六份自然纹理、CommonUI样式和Widget；平台机制无项目路径，非竞技界面不新增竞技依赖。Monolith同时修改真实按钮状态画刷及项目样式CDO，避免ThemeBindings运行时覆写回旧视觉。bPreserveFontSize继续开启，无ScaleBox或字体缩放；固定尺寸的输入保持284×42。

地图缩放命令仅改变本地UV视野，倍率1/2/4；圆形裁剪、坐标与按钮生命周期在项目Minimap适配。旅行重新挂载作者CombatHUD并传播当前PlayerContext，清理旧控制器委托；未恢复上一世界临时HUD。进度与人工验收边界见工作空间Docs/Implementation/NatureUIFinalTheme.md和内容包MonolithGenerationManifest.json。

随后“色彩与原稿一致”要求将当前菜单、技能栏、地图框和左上头像框改为四张原稿的无损纹理及十一份UI取样材质，生成素材只保留兼容与制作记录。主题新增UI.Style.Text.Menu的原稿碧绿文案；页面绑定保留字号。DefaultThemeDefinitionId仍保持空值，当前外观由已保存控件画刷和样式提供，不宣称默认主题异步服务已在客户端启用。

本轮追加主题回归发现两处Buttons.Add(Buttons[index])容器别名会在UE5.8触发断言，测试现先复制独立值再Add，确保歧义/重复校验路径真实执行；不改变生产解析规则。此项与真实Widget重载、Cook及人工视觉验收分别留证。

本轮登录面板新增UI.Style.Button.LoginOverlay透明按钮规则，资源归DBAUIPack_Core。原稿RGB不再加蓝色/透明乘色，属性以#FFFFFFFF回读验证；600×315面板与284×42输入框为固定尺寸。第五张原稿和第十二份取样材质的证据见NatureUIFinalTheme.md及内容包清单。DefaultThemeDefinitionId保持原值，不能宣称主题服务已启用。密码按住显隐只读输入控件，释放/失活恢复遮罩与清空，不进入统一视图状态。
