# WorldHUD（世界HUD）

当前 World HUD 的真实接线：

- Region/World/Experience：来自ApplicationFlow Assignment（世界分配投影）。
- Health/MaxHealth：来自当前Pawn上的 GamePlatformCombatComponent 只读接口。
- Shield/MaxShield：来自同一Combat只读接口。
- InteractionPromptId：来自 GamePlatformInteractorComponent 的 OnFocusChanged（焦点变化）事件与CurrentFocus。
- 更新方式：事件驱动，无项目Tick扫描。

当前尚未接入真实项目客户端数据源：

- AbilityIds：项目真实Ability资产仍为0。
- StatusIds：尚无统一UI-safe Status快照端口。
- QuestObjectiveIds：GamePlatformQuestClient有缓存接口，但当前项目没有把复制Quest Snapshot正式喂入该客户端缓存。

因此这三个数组保持空，状态为“未执行”，不允许UI扫描ASC/Actor或虚构数据。
