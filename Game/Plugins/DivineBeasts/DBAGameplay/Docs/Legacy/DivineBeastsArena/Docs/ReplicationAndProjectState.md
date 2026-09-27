# ReplicationAndProjectState（复制与项目状态）

项目不复制完整Mode Definition。

通用GamePlatformArena继续复制：
Phase、TeamState、ObjectiveState、Result、PlayerState公开比赛状态。

本轮项目Revision主要在服务器Assignment门禁使用；Production规则尚未配置，因此没有新增项目复制组件/第二GameState。

身份复制修复：
- PlayerState PlayerIdPublic继续公开。
- CharacterId仅OwnerOnly复制。
- HeroDefinitionId继续作为本局公开选人身份。
- CharacterId与HeroDefinitionId不再混用。

未来确需客户端显示ProjectRuleRevision时，只复制ID/Revision，不复制Definition全文。
