# DBAGameplay（神兽联盟双端玩法插件）

正式位置：`Game/Plugins/DivineBeasts/DBAGameplay/`。本插件只拥有神兽联盟项目通用身份与角色规则，不实现平台通用机制；项目竞技定义已归独立可选DBAArena。

| 模块 | 端侧 | 当前职责 |
| --- | --- | --- |
| `DivineBeastsRuntime` | 双端 | Shared生成身份、角色/体验目录与项目上下文校验 |
| `DivineBeastsCharactersRuntime` | 双端 | 项目英雄定义、生肖身份、创建提供者、出生初始化，以及项目专属 Momentum（气势）Definition/AttributeSet |
| `DivineBeastsAbilitiesRuntime` | 双端 | 生肖技能逻辑定义、每级数值校验、可信 GAS 授权/撤销、OwnerOnly（仅拥有者）技能状态复制及项目可玩角色组合；2026-10-10 已通过授权组件的 UE5.8 单文件定向编译，**全模块 DLL 与正式技能联机尚未验收** |

依赖方向为项目层到 `GamePlatformCore`（平台核心）、`GamePlatformCharacter`（平台角色）、`GamePlatformAbilitySystem`（平台技能）、`GamePlatformData`（平台数据）和 `GamePlatformCombat`（平台战斗），不依赖 `GamePlatformArena`（竞技模块）。Shared生成目录可以包含中立模式ID与角色映射，但不因此链接竞技实现。应用流程、UI、在线服务、VFX执行器及MOBA表现语义不归本插件。Momentum（气势）是项目 Gameplay 真值：服务器在角色初始化时确保项目气势 AttributeSet 存在并按 Hero Definition 初始化，客户端只接收 GAS 复制。

核心属性分层保持：生命/护盾、攻击、防御、控制/韧性归 `GamePlatformCombat（游戏平台战斗插件）`；项目层只增加 `UDivineBeastsMomentumAttributeSet（神兽联盟气势属性集）` 与 `FDivineBeastsMomentumDefinition（气势定义）`。当前工程规则禁止恢复历史 Element（旧五行玩法）、克制、破元、共鸣，本插件不得以新文件名或新插件形式绕过该约束。

所有既有模块名、反射类型和导出宏均保持原值。项目身份与模式映射由 `Shared/Contracts` 生成；模块不会维护第二份 ID 表。当前真实 C++ 源码已通过 UE5.8 `DivineBeastsArenaEditor` 定向模块编译；本轮新增 Momentum AttributeSet/Definition、角色初始化接线和 UI 状态投影也已随 `GamePlatformCombat + DivineBeastsCharactersRuntime + DivineBeastsUIClient` 的 40 个构建动作成功编译。Dedicated Server 角色链继续通过 `GamePlatformCharacterInitializationExecutor` 与项目 `FDivineBeastsCharacterSpawnInitializer` 接通。Automation 用例本轮在进入测试队列前被本机 VisionOS SDK 缺失 `MainVersion` 的平台校验阻断，不把该环境阻断误报为新增用例失败。

原六插件文档和描述快照见 `Docs/Legacy/`，只用于追溯，不是现行架构规则。旧 `DivineBeastsContracts` External静态库规则未被任何运行时代码使用，且仓库无相应构建脚本或产物；其构建规则已归档，不作为活动模块。

### 2026-10-09 生肖技能数据驱动专项

新增 `DivineBeastsAbilitiesRuntime`（生肖技能运行模块）承载服务器安全的玩法定义、数值 DataTable（数据表）结构、技能授权组件、项目可玩角色及自动化测试源码；`DivineBeastsCharactersRuntime`（角色身份模块）只新增可空 `DefaultAbilitySetId`（默认技能集逻辑编号），不引入技能类或图标硬引用。竞技服务端已调整出生类；通用 OpenWorld/Village（常驻世界/新手村）出生路径和正式技能资产尚未实装。此增量在2026-10-10已完成 `DivineBeastsAbilityLoadoutComponent.cpp`（技能授权组件）的UE5.8编辑器目标单文件编译，UBT返回退出码0；整模块DLL链接、编辑器重新加载、双端构建及联机仍未完成，不覆盖上文历史批次的成功构建记录。详见 [AbilityDataDrivenArchitecture.md（技能数据驱动架构）](Docs/AbilityDataDrivenArchitecture.md) 和 [专项实施记录](../../../../Docs/Implementation/十二生肖技能数据驱动实施记录_20261009.md)。
