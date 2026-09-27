# ManualReview（人工审查）

状态：待人工审查。AI 未代签。

人工审查必须在真实 UE5.8 环境覆盖：Agent/Capsule一致性、ProjectPoint、无路径、Partial策略、Area/Filter成本、Dynamic Modifier、SmartLink到达/失败、Invoker Tile、World切换、World Partition、AI Patrol/Chase/ReturnHome、10+ Agent并发、Dedicated Server NavData、Client/Server Cook。

当前 0 个 `.uasset/.umap`；没有 NavMeshBounds/Modifier/SmartLink/WorldPartition 测试地图。因此这些运行项必须保持未执行。

如果后续引入客户端路径预览，还需人工确认其 Advisory 结果不会被用于服务器 Teleport、Combat、Interaction 距离或权威 AI。
