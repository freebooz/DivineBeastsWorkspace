# Troubleshooting（故障排查）

服务器加载Assignment失败：
先检查Production Catalog是否仍NotConfigured；当前这是预期Release阻塞，不应改成使用FoundationTest默认规则。

Hero Selection被拒绝：
检查HeroDefinitionId是否属于12生肖Catalog、真实Hero Definition资产是否已注册、可信Entitlement/Maintenance Provider是否唯一注册。

服务器提示GameplayLifecycleAdapter缺失：
当前平台尚无正式Spawn/Respawn实现，项目Server故意Fail Closed。禁止临时在Arena里直接SpawnActor/Possess绕过。

Ticket准入失败：
检查PlayerId、CharacterId、MatchId、DestinationServerId是否与Assignment完全一致，以及Ticket是否已消费/过期。

Result被拒绝：
检查MatchResult PlayerId/CharacterId/TeamId是否与原Assignment Roster一致。

UE/Go/Cook失败不能用静态脚本通过替代。
