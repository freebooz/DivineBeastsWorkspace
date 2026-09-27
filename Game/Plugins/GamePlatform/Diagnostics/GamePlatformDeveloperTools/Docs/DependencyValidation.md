# DependencyValidation（依赖验证）

UGamePlatformDependencyValidator 扫描当前 Build.cs 字面量模块依赖，建立模块图并检查：
- GamePlatform 只能依赖 GamePlatform 或引擎/第三方允许依赖。
- MobaCommon 可依赖 GamePlatform/MobaCommon。
- DivineBeasts 可依赖 GamePlatform/MobaCommon/DivineBeasts。
- 禁止反向依赖。
- 使用 DFS 状态栈输出完整循环路径 A -> B -> C -> A。

`GP.InheritanceBoundary（三层继承边界）`补充检查C++公开类型图：GamePlatform不得继承/公开引用MobaCommon或DivineBeasts，MobaCommon不得继承/公开引用DivineBeasts，跨层只能继承低层Public类型。PowerShell镜像门禁位于`Tests/Architecture/InheritanceBoundaryAudit.psm1`，当前真实源码扫描通过；二进制Blueprint/DataAsset父类待真实资产出现后由UE DataValidation补证。

`Tests/Architecture/ValidateDesignBaseline.ps1` 汇总插件结构、声明装配与继承边界；`PluginCompositionAudit.psm1`负责目标闭包，`InheritanceBoundaryAudit.psm1`负责源码继承/Public API。静态扫描不冒充UBT、UHT、AssetRegistry或Cook完整求值。