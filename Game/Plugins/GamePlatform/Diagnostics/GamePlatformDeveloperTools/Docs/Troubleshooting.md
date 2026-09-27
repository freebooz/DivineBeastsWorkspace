# Troubleshooting（故障排查）

DataValidation 未运行：确认 UE_ROOT 指向 UE5.8 根目录，检查 Engine/Binaries/Win64/UnrealEditor-Cmd.exe。

编辑器中看不到菜单：确认 DivineBeastsArenaEditor Target 为非 Shipping，并启用了 GamePlatformDeveloperTools；Commandlet 模式本来就不注册 UI。

Client/Server Leak 显示未执行：必须提供真实 Build/Cook 工件目录，而不是源码目录。

Release Gate 被阻断：逐项查看 Architecture、DataValidation、CookLeak、SecretScan、DebugShippingLeak 的真实退出码与证据。

人工 Review 无法 Passed：需要真实 Reviewer 显式确认并提供 Evidence。