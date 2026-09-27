# ConfigurationAndRun（配置与运行）

`DefaultEngine.ini`已配置 `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient`，用于 CommonUI 输入路由。

实际运行前还需要由 Unreal Editor 创建 Root Layout（根布局）Blueprint，并为 Screen/Modal/System/Loading/Debug 绑定 `UCommonActivatableWidgetStack`，为 HUD/Notification 绑定 Overlay。

项目 Composition Root（组合根）启动时应取得本地玩家的 `UGamePlatformUIManagerSubsystem`，安装 Root Layout，然后注册平台/项目 Screen Definition 和 Route Definition。

`Build/Validation/VerifyGamePlatformUI.ps1（平台UI综合验证脚本）`会从 `UE_ROOT（UE根目录）`定位 `Build.bat（UE构建脚本）`与 `UnrealEditor-Cmd.exe（UE命令行编辑器）`。工具链可用时，它使用独立 RunId（运行编号）与日志目录依次构建 `DivineBeastsArenaEditor（编辑器目标）`、`DivineBeastsArenaClient（客户端目标）`、`DivineBeastsArenaServer（服务器目标）`，Editor 构建通过后再执行 `GamePlatform.UI.*` UE Automation（UE自动化）；所有进程都有超时、真实退出码和仅终止自身进程的保护。当前 Runner 未配置 UE_ROOT，因此本轮这些运行项保持“未执行”。
