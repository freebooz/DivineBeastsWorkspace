# MigrationAndHandover（迁移与交接）

后续插件如需增加调试能力，应优先：
1. 在所属业务插件提供最小、稳定、只读的 Public Diagnostic Adapter（公开诊断适配器）。
2. 在 GamePlatformDebug（调试插件）注册新的 IGamePlatformDebugStateProvider（状态提供者）或扩展现有 Provider。
3. 复用 FGamePlatformDebugSnapshot（调试快照）和 Sensitive Filter（敏感过滤）。
4. 不把业务修改能力搬进 Debug。
5. 不让 Runtime（共享运行模块）依赖 ServerOnly/ClientOnly/Editor（服务器专用/客户端专用/编辑器）模块。

若未来确实需要 Development-only Server Snapshot RPC（仅开发服务器快照RPC），必须单独补齐 Auth、Rate Limit、Target Scope、Field Allowlist、Max Payload、RequestId、Timeout、Sensitive Filter（认证/限流/目标范围/字段白名单/负载上限/请求编号/超时/敏感过滤）以及真实 Shipping Binary Scan（正式发布二进制扫描）证据。

下一插件 GamePlatformDeveloperTools（游戏平台开发者工具插件）不在本轮范围。