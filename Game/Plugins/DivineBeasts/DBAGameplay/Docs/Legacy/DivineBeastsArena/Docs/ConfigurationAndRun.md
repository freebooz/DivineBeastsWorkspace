# ConfigurationAndRun（配置与运行）

静态专项验证：

powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsArena/TestArenaProjectConfig.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsArena/TestArenaModes.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsArena/TestArenaHeroSelection.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsArena/TestArenaResult.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/DivineBeastsArena/TestArenaPostMatch.ps1

综合验证：

Build/Validation/VerifyDivineBeastsArena.ps1

Production配置当前不允许通过。正式Release前必须补齐：
- 五模式正式MapId。
- Selection/Spawn/Respawn/Score/Win Policy ID。
- TimeLimit。
- ProjectRuleRevision。
- ContentRevision。
- PickBan显式Supported/Unsupported。
- DuplicateHero显式策略。
- Overtime/SuddenDeath显式Supported/Unsupported。
- 12个Hero Definition真实资产和可信Entitlement/Maintenance Provider。
- 平台GameplayLifecycleAdapter真实实现。

UE5.8、Go、PostgreSQL、Dedicated Server环境不存在时必须报告未执行。
