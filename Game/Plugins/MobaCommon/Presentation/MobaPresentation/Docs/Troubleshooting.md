# Troubleshooting（故障排查）

无表现请求：先确认MobaPresentationClient已加载、LocalPlayer存在、事实源已绑定，再检查Semantic是否注册。

ProviderMissing：当前表示平台Coordinator存在，但没有Provider处理该Semantic；这是安全降级，不是Gameplay失败。

Travel后旧表现出现：检查WorldGeneration是否随World切换递增，事实是否携带旧Generation。

Respawn后旧Pawn表现出现：检查TargetEntityId和AvatarGeneration。

比分表现重复：检查Arena Team Revision/Player StatsRevision和FactId去重，不要改成Timestamp去重。

UE编译失败时以锁定UE5.8源码API为准，不用静态脚本结果替代编译证据。
