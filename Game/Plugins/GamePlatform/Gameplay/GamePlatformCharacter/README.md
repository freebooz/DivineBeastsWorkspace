# GamePlatformCharacter（游戏平台角色插件）

跨游戏角色Definition（角色定义）、基础移动/出生包络、角色初始化契约、角色创建Provider（提供者）接口、稳定角色状态只读接口以及统一Definition异步加载。Server-safe Definition（服务器安全定义）不硬引用Mesh/VFX/SFX/UI/具体Ability类。

插件当前只保留一个双端`GamePlatformCharacter` Runtime（运行时）模块。原`GamePlatformCharacterClient`只有空模块入口且没有独立职责，已按“禁止无职责空模块”规则移除；客户端角色外观、动画、相机和UI继续由各自Presentation/Animation/Camera/UI领域负责，避免角色插件重新形成表现层耦合。

统一Spawn Operation（出生操作）通过`GamePlatform.CharacterInitializer`模块化特性发现项目初始化器；项目实现不得自行SpawnActor或Possess。真实Spawn/Possess仍由`GamePlatformGameplay`统一生命周期实现完成。
