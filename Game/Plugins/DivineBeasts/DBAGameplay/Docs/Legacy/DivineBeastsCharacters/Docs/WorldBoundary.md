# WorldBoundary（世界边界）

Characters不实现OpenWorld/Village世界规则、不决定出生点、不管理Shard/Instance。

World/Gameplay Spawn流程在确定出生位置前应查询Hero Definition SpawnEnvelope，使用正确Capsule范围做空间校验；随后由平台Spawn Operation创建ACharacter并调用项目Initializer。

同一Characters插件服务OpenWorld.Hub、OpenWorld.Main、Village.Main/Tutorial/Training以及未来MainArena。
