# MigrationAndHandover（迁移与交接）

实施前唯一 `GamePlatformNavigation`位于 `GameFoundation/World`且只有空 Runtime/Server 模块入口；本轮迁移同一插件身份到规范指定 `GameFoundation/Gameplay/GamePlatformNavigation`，并同步 plugin-catalog 与总体架构。

上一轮 AI 的直接 NavigationSystem 依赖和 `GetRandomReachablePointInRadius`调用已迁移到 NavigationServer 服务；AI Runtime 共享模块仍不依赖导航执行模块。

BehaviorTree 原生 Move To 没有被重写；后续开发不要为了“纯接口”把原生 PathFollowing 复制成第二套移动系统。

后续项目层要新增 Area/Filter/SmartLink Traversal，只应扩展稳定 ID 和适配器，不允许 Navigation 反向依赖 AI/Interaction/Combat/DivineBeasts。
