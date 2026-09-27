# Development（任务开发验证资源）

此目录用于由 Unreal Editor（虚幻编辑器）真实创建的 Development Quest Definition（开发任务定义）：

- `DA_Quest_FoundationTutorial`（基础教学任务定义）
- `DA_Quest_FoundationInteraction`（基础交互任务定义）
- `DA_Quest_FoundationCombat`（基础战斗任务定义）

当前 Runner 没有可用且锁定的 UE5.8 工具链，因此本轮没有伪造任何 `.uasset/.umap`。

真实 Definition（定义资产）必须由 Unreal Editor 创建，并用于验证 Region → Interaction → Combat → Completion → PlayerData → PostgreSQL → Outbox 完整链。
