# NavigationInvokers（导航调用器）

AgentProfile 中 `InvokerPolicy=RegisterWhenActive`时，服务器服务可调用 UE `RegisterNavigationInvoker`/`UnregisterNavigationInvoker`，并用 Actor 弱引用防重复注册和 Actor Destroy 清理。

Generation/Removal Radius 来自受信 Profile，并受全局安全上限约束；不会采用文档示例半径作为所有地图固定值。

插件不会全局打开“Generate Navigation Only Around Navigation Invokers”。UE 文档明确 Invoker 仅在项目/地图选择该生成策略时用于 Agent 周围导航生成。

当前真实 Invoker Tile 生成/移除、大量 Invoker 性能均未执行。
