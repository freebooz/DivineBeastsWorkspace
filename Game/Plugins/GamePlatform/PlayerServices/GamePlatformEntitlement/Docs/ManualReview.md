# ManualReview（人工审查）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

状态：未执行。AI 未代签。

人工审查至少应覆盖：两个 UE 模块构建、Server 不链接 EntitlementClient、Client/Server Cook、0003 Migration、永久/临时/Future/Expired Grant、多来源聚合、单来源 Revoke、全部 Revoke、并发 Grant/Revoke、Outbox、Quest Reward 一次性、Gateway 玩家隔离、普通 Client 无 Grant/Revoke、Dedicated Server Hero 授权、Skin 授权边界、服务重启与 0/10/100/1000 权益规模。

当前缺少正式 PlayerAuthenticator、ServerAssignment 验证器、消息订阅器以及 Go/PostgreSQL/UE5.8 运行环境，因此运行项保持未执行。
