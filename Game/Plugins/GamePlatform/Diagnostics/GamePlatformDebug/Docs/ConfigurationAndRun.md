# ConfigurationAndRun（配置与运行）

非 Shipping（正式发布）构建中可用。

常用入口：
- gp.Debug.Status（调试状态）。
- gp.Debug.World / Character / Ability / Combat / AI / Navigation / Network / Session / Loading / Telemetry（各类只读摘要）。
- gp.Debug.Panel（客户端面板）。
- ' 键进入 Gameplay Debugger（玩法调试器），再选择 GP.* 分类。

可变调试命令默认关闭：
gp.Debug.AllowMutating=0。
需要低风险调试动作时，开发人员显式设置 gp.Debug.AllowMutating=1。

gp.Debug.Remote（远程调试开关）默认 0；V1 没有任意 Remote Query RPC（远程查询RPC），该变量不解锁隐藏的生产能力。

Network Trace（网络追踪）可使用 -trace=net -NetTrace=1 启动参数；实际命令以锁定 UE5.8 源码与运行验证为最终依据。

Unreal Insights（虚幻分析器）直接复用 UE5.8 Trace（追踪）系统：
- Trace.Start（开始追踪）。
- Trace.Stop（停止追踪）。
- Trace.Status（追踪状态）。
- Trace.File（写入追踪文件）。
- Trace.Send（发送到追踪服务）。

GamePlatformDebug（游戏平台调试插件）只桥接不需要任意文件路径/URL的 Trace.Start / Trace.Stop / Trace.Status。Trace.File / Trace.Send 继续直接使用 UE 原生命令，不由 `gp.Debug.*` 接收任意路径或地址参数，避免把调试插件扩展成文件/URL执行入口。