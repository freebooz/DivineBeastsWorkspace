# SecurityAndAuthority（安全与权威）

UE `GamePlatformInventoryClient（背包客户端模块）`不再持有 AccessToken。默认 Transport 只持有 `UGamePlatformOnlineClientSubsystem`弱引用，Authorization、Token Refresh、同源校验、HTTP Attempt Timeout 和取消由 Online 统一负责。背包日志只记录状态、稳定错误、Revision 与 OperationId，不记录 Token、密码或数据库信息。

Gateway 的 `requireAuth（认证中间件）`从 Bearer Access Token 得到可信 `AuthenticatedSession.PlayerID`；`/v1/inventory...`公网请求结构不存在 playerId 字段。Gateway 调用 PlayerData 内部 HTTP 时才附带已经认证的 playerId，因此客户端不能通过修改 Body 越权读取其他玩家背包。

PlayerData 的现有内部 HTTP 通道属于 Backend 服务间边界，本轮没有为 Inventory 单独发明另一套内部 Token。部署时必须继续保证 PlayerData internal 端点不直接暴露公网；如果后续统一引入服务间 mTLS/Internal Bearer，应在 Backend 公共传输层一次性实现，不在 Inventory 私建旁路认证。

所有长期写操作由 PlayerData Inventory Domain + PostgreSQL Repository 权威执行。客户端只提交 OperationId、ExpectedRevision 和操作参数；客户端 Definition、UI 数量和 Slot 均不具有权威性。OperationId 同键异内容会拒绝，RevisionConflict 会触发客户端全量对账。

普通客户端没有 Grant/Consume。可信 Dedicated Server 的 Grant/Consume、Quest Reward、External Pickup 仍待后续在真实 GameServerControl/Session 服务器实例—玩家绑定校验基础上实现；当前文档不得将其写成已完成。货币属于 Economy（经济钱包），装备槽位/能力应用属于 Equipment（装备），永久权益属于 Entitlement（权益），均不并入 Inventory。
