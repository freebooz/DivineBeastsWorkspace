# API（接口）

Runtime公共API：
- FDivineBeastsArenaProjectModeSpec（项目竞技模式规格）。
- UDivineBeastsArenaModeDefinition（项目竞技模式Definition）。
- FDivineBeastsArenaModeCatalog（五模式结构Catalog）。
- IDivineBeastsArenaTrustedHeroEligibilityProvider（可信英雄资格提供者）。

Client公共API：
- UDivineBeastsArenaClientSubsystem::GetProjectArenaModeIds。
- BuildMatchmakingRequest：仅ProductionReady模式允许构建请求。
- RequestPostMatchReturnToWorld：复用ApplicationFlow重新申请OpenWorld。

Server公共API：
- FDivineBeastsArenaServerProjectExtension，实现平台Project Extension与HeroEligibility Provider。

没有项目MatchResult DTO、新ArenaHeroId、客户端Winner/Score/Team设置接口或后端DTO公共泄漏。
