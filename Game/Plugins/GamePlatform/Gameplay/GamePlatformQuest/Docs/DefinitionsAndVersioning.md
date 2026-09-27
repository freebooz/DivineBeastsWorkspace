# DefinitionsAndVersioning（定义与版本）

`UGamePlatformQuestDefinition`字段包括 QuestId、Version、QuestTags、Objectives、Prerequisites、RepeatPolicy、TimeWindowPolicy、RewardSetId、bCanAbandon 和 RequiredDefinitions。Definition 不包含 UI Widget、Niagara、数据库连接、Go 路由或玩家当前进度。

`ValidateDefinition`拒绝空 QuestId、非法版本、空 Objectives、重复 ObjectiveId、非正 RequiredValue、Region Objective 缺少 RegionId、自依赖和第一版不支持的 Periodic 时间策略。Server Registry 额外通过 DFS（深度优先）校验跨 Quest Prerequisite 循环。

玩家从 PlayerData 恢复时：Completed（已完成）历史事实即使当前 Definition Version 改变也保留，不允许倒退；尚未完成的任务若定义缺失或版本不一致则明确返回 DefinitionMissing/DefinitionVersionMismatch，要求迁移或人工定义兼容，不静默覆盖。

Development Definition 必须由 Unreal Editor 真实创建在 Content/Development；当前没有伪造 DA_Quest_FoundationTutorial/Interaction/Combat `.uasset`。
