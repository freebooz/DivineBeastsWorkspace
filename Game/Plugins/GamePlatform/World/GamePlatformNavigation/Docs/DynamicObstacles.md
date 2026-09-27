# DynamicObstacles（动态障碍）

`UGamePlatformNavigationModifierComponent`继承 UE `UNavModifierComponent`，通过平台 AreaKind 切换 Default/HighCost/Blocked Area Class。它不依赖 GamePlatformInteraction（交互插件）。

典型 Door 状态变化应由 Door/世界对象自身或主工程适配器调用 Modifier Area；Navigation 只负责导航语义，不读取 Door 业务私有状态。

UE Nav Modifier 负责向 Navigation Octree/NavMesh 提供区域修改；实际是否动态重建依赖地图 Runtime Generation 配置。

本轮没有测试地图，因此“关门后阻挡/高成本、开门后路径恢复”和 Dynamic Rebuild CPU 证据均未执行。
