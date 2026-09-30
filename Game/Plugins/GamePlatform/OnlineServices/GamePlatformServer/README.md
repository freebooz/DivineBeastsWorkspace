# GamePlatformServer（游戏平台服务器控制面生命周期）

## 1. 插件定位

GamePlatformServer 是 GamePlatform（游戏平台基础层）的服务器控制面通用插件，只提供 Dedicated Server（专用服务器）的中立生命周期机制：Register（注册）、Heartbeat（心跳）、Ready（可分配）、Drain（排空）和本地 Stop（停止）。

平台层只理解 GameId、GameServerId、ServerRoleId、ExperienceId、WorldId、RegionId、Endpoint、Capacity 等中立字符串或数值，不认识《神兽联盟》的 OpenWorld、Village、MainArena 具体枚举，也不依赖 MobaCommon（MOBA通用层）或 DivineBeasts（项目层）。项目专属角色、体验、地图、资源和 Ready Gate（就绪门禁）由第三层 DBAServer（神兽联盟服务器组合插件）负责。

依赖方向保持：DivineBeasts（项目层） → MobaCommon（MOBA层，可选） → GamePlatform（平台层）。GamePlatformServer 不反向引用上层。

## 2. 生命周期与断线恢复

UGamePlatformServerLifecycleSubsystem 按 GameInstance（游戏实例）隔离。Initialize（初始化）阶段不联网，只有组合根显式提交经过校验的 FGamePlatformServerInstanceInfo（服务器实例信息）后才访问控制面。

注册、Ready 和 Drain 控制操作对瞬时故障采用单请求串行的指数退避重试：初始约 0.5 秒，逐步退避并封顶约 8 秒，同时依据 GameServerId（服务器实例编号）加入稳定抖动，避免大量服务器在控制面恢复时同时重试造成惊群。401/403、协议错误、业务拒绝和本地配置错误视为永久错误并 Fail Closed（失败关闭）。

Heartbeat Pump（心跳泵）默认每 10 秒低频执行一次。平台层不统计玩家，只调用上层注入的玩家数提供函数。上一心跳仍在飞行时直接跳过当前周期，不排队、不形成请求积压。408/425/429/5xx、请求启动失败和传输失败视为瞬时心跳故障，保留当前生命周期并等待下一周期恢复；成功后连续故障计数归零。

BeginDrain（开始排空）优先级高于心跳：调用后先停止新的周期心跳并立即进入本地 Draining（排空中）语义，使项目层能够先阻止新准入。如果此时旧心跳仍在飞行，Drain 控制操作会延后到该心跳结束后自动发起。控制面 Drain 未确认、仍有请求在飞行或仍有重试任务时，CompleteDrain（完成排空）必须拒绝，避免“本地已经停止但控制面仍认为可分配”的竞态窗口。

## 3. HTTP 控制面安全与资源边界

默认 HTTP Provider（HTTP提供者）通过 Modular Features（模块化功能）注册，HTTP/JSON 仅是 Private（内部实现）依赖，不进入 Public API（公开接口）。

控制面请求具备以下边界：

- GAMESERVERCONTROL_INTERNAL_TOKEN（内部令牌）与 GAME_SERVER_ID（服务器实例编号）从受控进程环境读取，不写入 Profile、资产或日志。
- GAMESERVERCONTROL_REQUEST_TIMEOUT_SECONDS（控制面单请求超时）可选，默认 5 秒，仅接受 1～30 秒范围。
- Shipping（正式发布）构建只允许 HTTPS（加密HTTP）控制面地址；开发构建允许本地 HTTP 联调。
- 携带 Bearer（持有者令牌）的请求必须拒绝自动重定向，避免凭据被转发到非预期主机。
- AcceptedResponse（接受响应）正文采用流式接收并限制为 16 KiB；超过限制立即终止继续接收，避免异常响应扩大服务器内存。
- 只接受 2xx 且 JSON 中 accepted=true；错误响应体不写日志，避免服务端回显敏感信息进入日志。
- 408/425/429/5xx 映射为可恢复错误；401/403 单独映射为授权错误，其他 4xx 作为业务或协议拒绝处理。

## 4. 与《神兽联盟》的组合关系

DBAServer（神兽联盟服务器组合插件）负责：

- 读取 ServerRole Profile（服务器角色配置）并验证允许的 Experience（体验）。
- 验证当前 World（世界）、地图和必需服务器资源已经满足 Ready Gate。
- 从部署环境组装 GameServerId、RegionId、BuildVersion、PublicEndpoint、Capacity 等实例字段。
- 使用权威 AGameModeBase::GetNumPlayers（游戏模式在线玩家数）向 Heartbeat Pump 提供当前玩家数。
- 在项目本地清理完成后调用 BeginDrain / CompleteDrain。

GamePlatformServer 不包含 OpenWorld、Village、MainArena 的硬编码分支，因此可以被其他 Dedicated Server 项目复用。

## 5. 性能原则

插件不使用逐帧 Tick（帧更新）轮询。心跳使用低频 CoreTicker（核心定时器），同一 GameInstance 只允许一个 Heartbeat Pump；控制操作与心跳都保持单飞请求，不建立无界队列。响应体、字段长度、请求超时和重试间隔均有上限。

环境变量按请求读取，允许控制面令牌轮换；其频率仅为低频生命周期/心跳路径，不属于战斗热路径。玩家数统计由项目层在心跳周期读取一次，不在 Tick、GAS（Gameplay Ability System，玩法能力系统）或战斗路径增加开销。

## 6. 验证

Private/Tests/GamePlatformServerLifecycleTests.cpp（原生自动化测试）覆盖服务器实例信息边界。Tests/Scripts/TestServerArchitecture.ps1（服务器控制面架构门禁）检查三层反向依赖、ServerOnly（仅服务器）装配、纯代码插件、HTTP/JSON私有依赖、超时、禁止重定向、响应大小上限、心跳恢复、控制操作退避和 Drain 门禁。

Shared/Contracts/GamePlatform/OpenAPI/game-server-control.openapi.yaml（共享OpenAPI契约）仍是 HTTP 路径和字段的协议事实来源。

## 7. 部署注意事项

控制面必须在后端侧真实执行 Heartbeat TTL（心跳过期）或等价健康淘汰，不能只依赖 Dedicated Server 主动上报。GamePlatformServer 可以保证客户端侧持续发送和恢复，但无法替代后端对长期失联实例的过期摘除。若后端未落实该门禁，仍可能出现失联服务器被继续分配的问题，应作为控制面部署验收项单独验证。

## 2026-09-30 准入请求修复

HTTP准入提供者停止接纳后先解绑全部完成委托并取消自有请求，再在游戏线程以ServerAdmissionCancelled完成各操作；取消和关闭共享原一次终态门禁，完成回调不在锁内执行。模块卸载显式Shutdown，析构幂等兜底，启动失败也解绑委托。

响应体在接收阶段累计进独立共享缓冲，沿用原32KiB上限，超限停止接收并明确失败；不等全量GetContent后才做限制。接收回调不捕获Provider，完成回调显式CompleteOnGameThread。Private/Tests/AdmissionShutdownTests.cpp验证未发送HTTP请求的关闭/取消，Native接收预算验证增量边界；这些不替代真实慢响应、重定向、模块卸载与网络端到端测试。最终本轮证据见Docs/Implementation/GamePlatformDesignRemediation/ExecutionProgress.md（工作空间根）。
