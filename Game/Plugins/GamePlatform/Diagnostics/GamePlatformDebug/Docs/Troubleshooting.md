# Troubleshooting（故障排查）

看不到 gp.Debug.* 命令：
1. 确认不是 Shipping（正式发布）构建。
2. 确认当前 Target（构建目标）已 EnablePlugins GamePlatformDebug（启用调试插件）。
3. 查看 gp.Debug.Status。

Gameplay Debugger（玩法调试器）没有 GP.*：
1. 确认 GameplayDebugger（玩法调试器）模块可用。
2. 确认分类在当前 Gameplay Debugger Settings（玩法调试器设置）中未被用户禁用。
3. 确认正在 PIE/SIE/Standalone（编辑器运行/模拟/独立运行）支持环境。

Provider 返回 N/A（不可用）：
这是明确的边界结果，通常表示相关业务插件尚未公开安全只读接口；不要用反射、私有字段或全量 Actor 扫描绕开。

客户端 Server View（服务器视角）没有本地数值：
这是预期设计。纯客户端不伪造服务器状态，请使用 GP.* Gameplay Debugger 分类查看权威复制视角。