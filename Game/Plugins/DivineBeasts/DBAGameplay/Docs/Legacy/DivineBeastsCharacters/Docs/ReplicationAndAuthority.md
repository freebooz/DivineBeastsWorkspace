# ReplicationAndAuthority（复制与权威）

UDivineBeastsCharacterComponent默认复制。

- CharacterId：COND_OwnerOnly（仅拥有者）。
- HeroDefinitionId：公开复制。
- ZodiacIdentity：公开复制。
- SpawnGeneration / AvatarGeneration：公开复制。
- bServerReady：公开复制。
- bPersistentCharacterIdRequired：OwnerOnly。

身份只能由服务器AuthorityBindTrustedContext绑定，拒绝比当前更旧的SpawnGeneration/AvatarGeneration。Definition异步回调同时检查DefinitionRequestGeneration、SpawnGeneration、AvatarGeneration，旧Pawn或旧请求结果不能污染新Avatar。

不复制Definition UObject、视觉资源路径、Backend Profile、Token或TransferTicket。
