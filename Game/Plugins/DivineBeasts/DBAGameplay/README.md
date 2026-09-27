# DBAGameplay（神兽联盟双端玩法插件）

正式位置：`Game/Plugins/DivineBeasts/DBAGameplay/`。本插件只拥有神兽联盟项目通用身份与角色规则，不实现平台通用机制；项目竞技定义已归独立可选DBAArena。

| 模块 | 端侧 | 当前职责 |
| --- | --- | --- |
| `DivineBeastsRuntime` | 双端 | Shared生成身份、角色/体验目录与项目上下文校验 |
| `DivineBeastsCharactersRuntime` | 双端 | 项目英雄定义、生肖身份、创建提供者和出生初始化 |

依赖方向为项目层到 `GamePlatformCore`、`GamePlatformCharacter`，不依赖 `GamePlatformArena`。Shared生成目录可以包含中立模式ID与角色映射，但不因此链接竞技实现。应用流程、UI、在线服务、VFX执行器及MOBA表现语义不归本插件。

所有既有模块名、反射类型和导出宏均保持原值。项目身份与模式映射由 `Shared/Contracts` 生成；模块不会维护第二份ID表。当前有真实C++源码与自动化用例，但本次尚未由UE 5.8构建。

原六插件文档和描述快照见 `Docs/Legacy/`，只用于追溯，不是现行架构规则。旧 `DivineBeastsContracts` External静态库规则未被任何运行时代码使用，且仓库无相应构建脚本或产物；其构建规则已归档，不作为活动模块。
