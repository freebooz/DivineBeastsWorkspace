# MigrationAndHandover（迁移与交接）

实施前 `GamePlatformAI`仅有两个空模块入口，没有 AIController、Perception、BT、Blackboard、Target Candidate、Patrol或Threat旧实现，因此无平行系统需要迁移。

正式 Owner保持 `Game/Plugins/GamePlatform/Gameplay/GamePlatformAI`；共享/ServerOnly模块身份保持原规划，不修改40插件名称。

本轮没有生产化 GamePlatformNavigation、Character、World 或完整 AbilitySystem，只通过现有公开边界消费真实可用能力；后续插件生产化时应保持依赖方向，不让 Combat/Character/Interaction反向依赖AI。

交接优先项：锁定UE5.8 Build，在Editor创建DA/BB/BT和NavMesh测试地图，完成真实BT/导航/GAS/Combat联调后再扩展StateTree、EQS或AI Interaction。
