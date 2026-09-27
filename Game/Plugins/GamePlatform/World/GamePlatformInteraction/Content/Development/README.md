# Development（交互开发验证资源）

此目录预留给 Unreal Editor（虚幻编辑器）真实创建的 Interaction Development（交互开发验证）资产和中立测试地图引用。

当前源码已经提供 C++ Development TestPawn、Door、Pickup、HarvestNode，但 Runner 没有可用且锁定的 UE5.8 工具链，因此本轮没有伪造任何 `.uasset/.umap`。

后续正式创建测试地图后，应验证 Focus、Instant、Hold、双玩家 Pickup 竞争、Harvest 取消、Late Join、Travel、断线和 Dedicated Server（专用服务器）权威。
