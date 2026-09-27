# Troubleshooting（故障排查）

返回 NavigationUnavailable：检查当前 World 是否存在 UNavigationSystemV1；NavDataMissing：检查 NavMeshBounds/SupportedAgent/RecastNavData 是否覆盖当前 Agent Profile。

InvalidAgentProfile：检查 Profile 是否注册且半径/高度/Invoker参数合法；角色仍卡墙时运行 `ValidateAgentProfileForActor`检查 Capsule/CharacterMovement NavAgent 是否大于 Profile。

InvalidFilter：检查 FilterId 是否已在当前 World Registry 注册；客户端不能直接上传 Filter Class 路径。

AI Patrol失败：确认 AIDefinition NavigationProfileId 已注册或使用默认 Agent，并检查地图是否有可达 NavData。SmartLink 卡住时检查原生 ReceiveSmartLinkReached 是否实际桥接到 NotifyTraversalRequested/CompleteTraversal。
