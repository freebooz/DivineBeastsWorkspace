# MigrationAndHandover（迁移与交接）

当前审计没有发现 Frontend/Plugins/GamePlatformUIClient 旧插件，也没有第二个正式 UGamePlatformUIManagerSubsystem（平台UI管理子系统），因此没有旧Owner迁移、Redirect（重定向）或弃用兼容任务。

正式Owner固定为 Game/Plugins/GamePlatform/Presentation/GamePlatformUI。项目层不得复制Manager、Layer Stack（层栈）、Loading Service（加载服务）、Input Router（输入路由）或Asset Manager（资产管理器）。

当前上层 DivineBeastsUI（神兽联盟项目UI插件）已经存在并复用 GamePlatformUIClient。它通过项目UI Contract（UI契约）和DBAClient Composition Root（项目客户端组合根）接业务，不要求GamePlatformUI反向依赖项目层。本轮没有修改项目页面业务。

本次平台生产化对上层的主要约束变化：

- 交互Screen/Modal/System页面需要合理DefaultFocusWidgetName。
- ScreenDefinition与Route注册现在会Fail Closed（失败关闭）校验Layer、路径、目标Screen和Tag冲突。
- HUD/Notification继续走Overlay入口，不允许作为Screen Definition注册。
- Travel额外清理HUD/Notification。
- Soft Widget/Preload/Platform Variant路径必须是安全本地Cook路径。

后续交接优先补UE5.8编译、Root Layout/模板二进制资产、Multi-PIE、Client/Server Cook与设备输入验证，而不是继续增加平台UI抽象层。

