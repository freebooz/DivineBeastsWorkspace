# TestingAndEvidence（测试与证据）

## 已有自动化覆盖

`Private/Tests/OnlineSessionTests.cpp` 直接编译历史纯逻辑生产内核，覆盖配置失败关闭、异步一次终态、并发登录所有权、Refresh single-flight（单次共享刷新）、旧 Token 401、403 不刷新、刷新结果不确定、Logout 迟到响应、请求取消、实例隔离、资料修订单调、并发/队列预算、有限重试和 Shutdown。

`GamePlatformOnlineAuthTests.cpp` 使用真实 `UGameInstance → UGamePlatformOnlineClientSubsystem → IGamePlatformOnlineService` 生产入口和可注入测试 Provider，覆盖公共门面注册、无效登录、并发登录、刷新、401 安全重放、读取瞬态错误重试、写请求幂等约束、非幂等写不自动重放、退出及实例清理等行为。

## 当前运行证据

当前规范路径下已重新执行原生 CMake 配置、MSVC 编译和 CTest，`OnlineLogicTests` 通过。该结果只证明纯逻辑回归，不代表 UE/UHT/HTTP 或真实后端已经通过。

本轮还使用 `F:\\UnrealEngine-5.8.0-release` 的 UE5.8 工具链启动 `DivineBeastsArenaEditor Win64 Development（神兽联盟编辑器 Win64 开发目标）` 完整构建。该构建触发 3929 个动作，在 1200 秒工具运行上限内执行到至少第 59 个动作后超时；已返回日志中未出现 GamePlatformOnline 编译错误，但没有成功退出码，因此只能记录为“构建未完成”，不能视为通过。后续应继续采用增量或目标化构建取得明确终态。

UE5.8 的 Editor/Client/Server 目标必须使用工作空间锁定引擎重新构建，并执行 `GamePlatform.Online.*` 自动化测试。构建或测试未取得终态成功证据时，不得写成“已通过”。

## 正式交付仍需验证

- 真实 HTTPS Gateway 登录、刷新、退出、资料读取/更新和业务受保护请求。
- 401/403/408/409/429/5xx、断网、超时、响应丢失、Retry-After 和大响应。
- Redirect Reject（拒绝重定向）与流式正文上限在目标 HTTP 后端上的真实能力。
- PIE 多 GameInstance 隔离、World 清理、Owner 销毁、切图和 GameInstance 反初始化。
- Client/Editor/Server 三目标构建与 Dedicated Server 不携带客户端 HTTP/OnlineClient 模块的边界检查。
- 登录 → 玩家资料 → 创建/选择角色 → GamePlatformSession → Village/OpenWorld 的端到端流程。

历史交接文档中的“尚未实现”描述只代表当时断点，不得覆盖当前源码和本文件的最新证据。