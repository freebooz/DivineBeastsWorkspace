# 神兽联盟主题接入规则

平台GamePlatformUIClient提供主题定义、解析、生命周期与兼容控件；MobaCommon只保留竞技语义。现有DivineBeastsUIClientSubsystem选择项目主题，真实皮肤与CommonUI样式归DBAUIPack_Core/UI/Themes、UI/Styles，不创建独立主题插件。

## 配置

项目Game/Config/DefaultGame.ini包含[DivineBeasts.UI.Theme]的DefaultThemeDefinitionId，目前为空。只有Monolith生成并验证实际主题后才填写GamePlatformDefinition:dba.ui.theme.default@1。未配置时保留原页面；失败不阻断登录、不发送HTTP或触发自动认证。

## 迁移

通过Monolith向真实Widget的ThemeBindings字段配置WidgetName、StyleId、Kind、bAllowParentFallback、bPreserveFontSize。名字必须来自当前Widget树，不猜测登录或背包按钮名。主要按钮语义UI.Style.Button.Primary，次要UI.Style.Button.Secondary，正文UI.Style.Text.Body，标题UI.Style.Text.Title，面板UI.Style.Panel.Default。平台只提供机制，项目数据选择具体颜色、字体与纹理。

默认空绑定不改变历史UButton类型、原有点击/焦点和密码字段；无需为了换皮肤重建业务类或全部蓝图。原生文本默认保留字号。CommonUI文本使用当前控件实例的只读样式快照以避免原Style重复覆盖字号；公共类CDO永不修改。

## 验收

源合同Tests/Architecture/ValidateUIThemeIntegration.py；原生测试前缀GamePlatform.UI.Theme。先验证一套真实主题和样板页，再逐个领域接入。所有蓝图/主题/样式由Monolith创建、编译、保存、重载和记录；具体实现进度见Docs/Implementation/UIThemeImplementation_20261010.md，不能由本说明推导实际运行或发布通过。
