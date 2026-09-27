# PickupAndHarvest（拾取与采集）

`EGamePlatformInteractionCommitKind::Consume`用于 Development Pickup：服务器唯一消费后 `bConsumed=true`、`bEnabled=false`、TargetRevision++，状态复制使 Late Join（晚加入）看到已消费结果；不产生永久 Inventory 物品。

`Harvest`维护复制的 RemainingCharges；Commit 时只在服务器将次数减 1，不低于 0，每次成功 Revision++，到 0 自动禁用。没有实现重启后持久化或正式采集经济。

`Toggle`用于 Development Door/Switch：服务器切换复制的 bToggleState 并 Revision++，不要求正式门动画。

主工程 Development 胶水提供 `AFoundationInteractionDoor/Pickup/HarvestNode`，均使用 Visibility 碰撞体；没有创建伪造 `.uasset/.umap`测试地图。
