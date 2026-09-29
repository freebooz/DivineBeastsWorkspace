# GamePlatformPCG（游戏平台程序化内容生成插件）

## 1. 定位

`GamePlatformPCG（游戏平台程序化内容生成插件）`是 DivineBeastsWorkspace（神兽联盟工作空间）第一层 `GamePlatform（游戏平台层）/World（世界机制）` 的跨游戏通用 PCG（程序化内容生成）能力。

正式路径：

```text
Game/Plugins/GamePlatform/World/GamePlatformPCG
```

插件不创建第二套 `ProjectPCG` 身份，也不把神兽联盟的桃花新手村、生肖、具体地图、具体植被网格、作物或道路美术下沉到平台层。

## 2. 当前真实基线

当前插件版本仍为 `0.1.0`，只保留两个正式模块：

- `GamePlatformPCG（PCG共享运行模块）`：Runtime（共享运行时）。
- `GamePlatformPCGEditor（PCG编辑器模块）`：Editor（编辑器专用）。

当前已经存在并应继续复用的能力包括：

- `UGamePlatformPCGProfileDefinition（PCG配置定义）`。
- `UGamePlatformPCGBakeManifest（PCG烘焙清单）`。
- `IGamePlatformPCGService（PCG运行服务接口）`。
- `UGamePlatformPCGWorldSubsystem（PCG世界子系统）`。
- `GamePlatformPCGInspection（PCG图与输出审查）`。
- `GamePlatformPCGPolicy（PCG安全与数值策略）`。
- `UGamePlatformPCGEditorLibrary（PCG编辑器工具库）`。
- 原生 PCG 请求生命周期、清理、输出指纹与有限用途审查。

当前保留 0.1.0 固定四节点链路作为 Legacy Development Fixture（旧开发夹具）与回退基线，同时已新增 1.0 Template Contract（模板合同）代码路径、Schema v1（属性协议）、P0～P9 Primitive（原语）、Domain ID（领域ID）、Priority（优先级）、通用环境 Definition（定义）、基础放置器 Actor（实体）和首批 M0/M1 自定义节点。真实生产 `.uasset` 模板、Gold Level（金标准关卡）、HiGen（分层生成）和 World Partition（世界分区）仍未完成。

## 3. 1.0 目标

1.0 将插件升级为“官方 PCG 之上的项目级操作系统底座”：

- 官方 PCG 负责采样、点运算、空间计算、未来 HiGen（分层生成）与 GPU（图形处理器）执行。
- GamePlatformPCG 负责 Schema（属性协议）、Primitive（原语）、Definition（数据定义）、Template（模板）、Priority（优先级）、Director（世界编排）、Bake（烘焙）、Validation（校验）和工程门禁。
- DivineBeasts（神兽联盟项目层）只通过 Data Asset（数据资产）、Graph Instance（图实例）、项目 Child BP（子蓝图）和 ContentPack（内容包）提供具体世界内容。

## 4. 固定架构决策

- 保留 `GamePlatformPCG + GamePlatformPCGEditor` 双模块，不机械拆成 Core/Nodes/World/Network/Connector/Parcel/Biome 等空模块。
- 不新增 `ProjectPCG` 或 `DivineBeastsPCG` 平行代码插件。
- MobaCommon（MOBA通用层）当前不增加 PCG 中间层。
- Schema v1 使用代码级稳定注册表和 `FGamePlatformPCGAttr（PCG属性访问器）`，禁止业务代码散落手写 `Pcg.*` 字符串。
- 运行时纯装饰与影响碰撞、导航、采集、出生、胜负的权威结果严格分离。
- Dedicated Server（专用服务器）不运行 Runtime Cosmetic（运行时纯装饰）PCG。
- 影响玩法的结果优先采用 Editor Generation（编辑器生成）→ Bake（烘焙）→ Validation（验证）→ 世界静态交付。
- HiGen（分层生成）、World Partition（世界分区）和 RuntimeDetail（运行时细节）在 M0/M1 基础闭环后再开放。
- `.uasset/.umap`必须由 Unreal Editor（虚幻编辑器）真实创建，不使用文本占位。

## 5. 三层职责

### GamePlatform（游戏平台层）

负责 Schema、原语、通用 Definition、节点、基础 Actor、WorldDirector（世界生成编排器）、优先级总线、Bake Manifest、Editor Validator（编辑器校验器）、CI（持续集成）门禁和执行生命周期。

### MobaCommon（MOBA通用层）

1.0 无 PCG 代码扩展。只有未来多个 MOBA 项目出现稳定共享的竞技场空间语义时再评审。

### DivineBeasts（神兽联盟项目层）

`DBAWorlds（神兽联盟项目世界插件）`负责项目世界组合与校验；真实地图、Graph Instance、Biome（群系）、Crop（作物）、Road（道路）、Enclosure（围合）、MeshSet（网格集合）等资产归各 `DBAWorldPack_*（世界内容包）`所有。

## 6. 实施入口

正式改造方案与执行计划：

- [Docs/改造方案与执行计划.md](Docs/改造方案与执行计划.md)
- [Docs/组件清单与使用说明.md](Docs/组件清单与使用说明.md) —— 面向程序、TA（技术美术）、地编、美术和测试的完整组件表、状态和使用方式。

编辑器模块当前真实能力与断点：

- [Source/GamePlatformPCGEditor/README.md](Source/GamePlatformPCGEditor/README.md)

全局架构仍以：

- `AGENTS.md（工程规则）`
- `Game/Plugins/插件开发规范.md`
- `Docs/Architecture/解决方案总体规划.md`
- `Docs/Architecture/游戏端插件清单设计.md`

为上位约束。

## 7. 当前状态

本轮已经完成第一批 M0/M1 源码落地，包括 Schema v1、P0～P9 原语与世界阶段、Domain ID、环境 Definition、PriorityCarve（优先级挖洞）规则、基础放置器/WorldDirector（世界编排器）、Template Contract（模板合同）、首批节点、SelectSpanMeshByLength（按跨度选择栏片），以及使用真实 UE5.8 `UPCGGraph` API 的 M0/M1 Foundation Template Generator（基础模板生成器）和 Commandlet（命令行工具），并保留旧四节点兼容路径。架构静态门禁与原生策略测试已通过；Foundation模板尚未实际执行落盘，Gold Level、UE Automation、Client/Server Cook、HiGen/World Partition 与性能验收仍未完成，未执行项目不得写成“已通过”。
