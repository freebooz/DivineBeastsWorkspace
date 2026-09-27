# MigrationAndHandover（迁移与交接）

实施前仓库没有旧 Health、Death、Damage GE、CombatComponent、AttributeSet 或 HitValidation，因此不存在需要迁移的第二套正式战斗系统。

原有 `GamePlatformCombatServer`仅为空模块入口且没有调用者，本轮按正式规划删除其文件和 descriptor 身份，权威逻辑统一放在 `GamePlatformCombat` Runtime 模块内按 Authority 执行。

AbilitySystem 只获得 Combat 所需最小 ASC/标签适配，README 已明确完整 AbilitySystem 仍是 scaffold，避免后续团队误认为它已经完成。

后续接入 Character 时应实现 Combatant 接口和 MovementBlocked 适配；后续项目战斗扩展可以提供具体公式/关系策略，但不能复制第二套 Health/Death 真源。
