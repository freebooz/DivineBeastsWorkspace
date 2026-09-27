# MigrationAndHandover（迁移与交接）

实施前唯一 Interaction 身份位于 `GameFoundation/World/GamePlatformInteraction`，且只有空 Runtime/Server 模块入口，没有公开API或业务数据。

本轮将同一插件目录迁移到规范指定的 `GameFoundation/Gameplay/GamePlatformInteraction`，目录索引和总体架构同步更新；空 `GamePlatformInteractionServer`模块从 descriptor/源码删除，权威逻辑统一放入 Runtime 模块。

GamePlatformGameplay 只新增最小 Active/AvatarGeneration Provider 适配，完整 Gameplay 生命周期仍未完成；主工程只增加 Development Interaction 测试胶水。

后续正式 Character/Input/Online 接入必须继续通过公开 Provider/组合层完成，不能反向让 Interaction 依赖这些上层或客户端专有模块。
