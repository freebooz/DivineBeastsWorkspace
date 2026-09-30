# DBAGameplay（神兽联盟双端玩法插件）

正式位置：`Game/Plugins/DivineBeasts/DBAGameplay/`。本插件只拥有神兽联盟项目通用身份与角色规则，不实现平台通用机制；项目竞技定义已归独立可选DBAArena。

| 模块 | 端侧 | 当前职责 |
| --- | --- | --- |
| `DivineBeastsRuntime` | 双端 | Shared生成身份、角色/体验目录与项目上下文校验 |
| `DivineBeastsCharactersRuntime` | 双端 | 项目英雄定义、生肖身份、创建提供者、出生初始化，以及项目专属 Momentum（气势）Definition/AttributeSet |

依赖方向为项目层到 `GamePlatformCore`、`GamePlatformCharacter`、`GamePlatformAbilitySystem`，不依赖 `GamePlatformArena`。Shared生成目录可以包含中立模式ID与角色映射，但不因此链接竞技实现。应用流程、UI、在线服务、VFX执行器及MOBA表现语义不归本插件。Momentum（气势）是项目 Gameplay 真值：服务器在角色初始化时确保项目气势 AttributeSet 存在并按 Hero Definition 初始化，客户端只接收 GAS 复制。

核心属性分层保持：生命/护盾、攻击、防御、控制/韧性归 `GamePlatformCombat（游戏平台战斗插件）`；项目层只增加 `UDivineBeastsMomentumAttributeSet（神兽联盟气势属性集）` 与 `FDivineBeastsMomentumDefinition（气势定义）`。当前工程规则禁止恢复历史 Element（旧五行玩法）、克制、破元、共鸣，本插件不得以新文件名或新插件形式绕过该约束。

所有既有模块名、反射类型和导出宏均保持原值。项目身份与模式映射由 `Shared/Contracts` 生成；模块不会维护第二份 ID 表。当前真实 C++ 源码已通过 UE5.8 `DivineBeastsArenaEditor` 定向模块编译；本轮新增 Momentum AttributeSet/Definition、角色初始化接线和 UI 状态投影也已随 `GamePlatformCombat + DivineBeastsCharactersRuntime + DivineBeastsUIClient` 的 40 个构建动作成功编译。Dedicated Server 角色链继续通过 `GamePlatformCharacterInitializationExecutor` 与项目 `FDivineBeastsCharacterSpawnInitializer` 接通。Automation 用例本轮在进入测试队列前被本机 VisionOS SDK 缺失 `MainVersion` 的平台校验阻断，不把该环境阻断误报为新增用例失败。

原六插件文档和描述快照见 `Docs/Legacy/`，只用于追溯，不是现行架构规则。旧 `DivineBeastsContracts` External静态库规则未被任何运行时代码使用，且仓库无相应构建脚本或产物；其构建规则已归档，不作为活动模块。
