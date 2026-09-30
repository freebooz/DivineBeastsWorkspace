# GamePlatformCharacter（游戏平台角色插件）

跨游戏角色Definition（角色定义）、基础移动/出生包络、角色初始化契约、角色创建Provider（提供者）接口、稳定角色状态只读接口以及统一Definition异步加载。Server-safe Definition（服务器安全定义）不硬引用Mesh/VFX/SFX/UI/具体Ability类。

插件当前只保留一个双端`GamePlatformCharacter` Runtime（运行时）模块。原`GamePlatformCharacterClient`只有空模块入口且没有独立职责，已按“禁止无职责空模块”规则移除；客户端角色外观、动画、相机和UI继续由各自Presentation/Animation/Camera/UI领域负责，避免角色插件重新形成表现层耦合。

统一 Spawn/Respawn 流程通过 `FGamePlatformCharacterInitializationExecutor（平台角色初始化执行器）` 解析 `GamePlatform.CharacterInitializer` 模块化特性；正式组合根要求初始化器数量恰好为 1，0 个或多个实现均 Fail Closed。Executor 只初始化已经创建的 `ACharacter`，不调用 `SpawnActor/Possess`。MainArena 已通过标准 `AGameModeBase::RestartPlayerAtPlayerStart` 接入该执行器；OpenWorld/Village 后续也必须复用同一执行器，禁止各项目再建第二套角色初始化入口。
