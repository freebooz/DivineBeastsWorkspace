# Troubleshooting（故障排查）

页面OpenScreenAsync失败：当前预期WBP_UI_*资产尚未由UE Editor创建，先确认WidgetClass软路径真实存在。

页面没有焦点：检查Screen Definition DefaultFocusWidgetName与Blueprint内部Widget名称是否一致，并使用CommonUI DesiredFocusTarget验证。

Loading进度为不确定：这是合法状态；当GamePlatformLoading没有Task时Progress=-1，禁止用假计时器补100%。

Matchmaking按钮返回Unavailable：当前ArenaClient没有正式匹配提交/取消端口，不是UI故障。

Arena Hero Select返回Unavailable：当前缺少正式客户端选人提交端口。

Training Reset返回Unavailable：当前没有安全的Village Training服务器命令Owner。

Quest/Ability/Status HUD为空：当前没有完整UI-safe项目投影，禁止UI扫描ASC/Actor填充。

Server Cook出现UI：检查Server Target是否仍显式禁用DivineBeastsUI，并运行TestUICook.ps1扫描真实Cook工件。
