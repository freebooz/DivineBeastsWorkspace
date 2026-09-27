# MobaData（MOBA数据模块）

`GamePlatformMobaData（MOBA数据模块）` 提供 `UGamePlatformArenaModeDefinition（竞技模式主数据资产定义）` 和可在无二进制资产环境中验证的内置模式规格。

模式数据包含TeamCount、TeamSize、TotalPlayers、MapId、Selection/Spawn/Respawn/Score/WinCondition Policy、TimeLimit、Overtime和Version。校验强制 `TeamCount * TeamSize == TotalPlayers`。