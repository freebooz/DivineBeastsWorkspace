# Troubleshooting（故障排查）

AI保持 Disabled：检查 AIStateComponent 是否存在、DefinitionAsset/DefaultDefinition 是否有效、AIDefinitionId、BT/BB软路径和BrainType；StateTree会明确 Unsupported。

AI看不到目标：检查目标是否有 `UGamePlatformAITargetComponent`、服务器 Stimuli Source登记、Sight半径/碰撞/遮挡及 TargetEligibility。Hearing需要游戏实际 ReportNoiseEvent；仅配置 Hearing 不会凭空产生声音。

AI有目标但不移动：检查测试地图 NavMeshBounds/NavData、正式 BT 中 GoalLocation 的原生 Move To 分支，以及 Pawn NavAgent；当前没有自研路径回退。

AI不攻击：检查 PrimaryAbilityTag 是否有效、ASC是否有匹配并可激活 Ability、是否Stun/Silence、距离和目标资格。AbilitySystem仍是最小实现时，未授予正式Ability会返回失败并按Backoff重试。
