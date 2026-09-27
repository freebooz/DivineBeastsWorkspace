# DependencyValidation（依赖验证）

UGamePlatformDependencyValidator 扫描当前 Build.cs 字面量模块依赖，建立模块图并检查：
- GameFoundation 只能依赖 GameFoundation。
- MobaCommon 可依赖 GameFoundation/MobaCommon。
- DivineBeasts 可依赖三层。
- 禁止反向依赖。
- 使用 DFS 状态栈输出完整循环路径 A -> B -> C -> A。

Tests/Architecture/Test-PluginLayers.ps1 仍是现有结构和字面量依赖主门禁；两者互补，不把静态扫描冒充 UBT 完整求值。