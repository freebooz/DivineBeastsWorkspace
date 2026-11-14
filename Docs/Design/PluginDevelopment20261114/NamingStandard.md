# SSLM 文件与资源命名规范

状态：本次新交付已执行；既有工程批量迁移待本方案人工确认。日期：2026-11-14。

## 核心规则

采用 **PascalCase＋类型前缀＋归属＋语义名称＋必要变体**。新建的人工维护文件和资源名称仅使用英文字母与数字，首字母大写，不使用下划线、空格或连字符。中文用于显示名称、注释和文档标题。目录同样采用职责明确的 PascalCase；已有顶层 Game、Shared、Backend、Build、Deploy、Tests、Tools、Docs 保持不变。

Epic 建议通过类型、名称、用途和可选变体辨识资产，并明确其示例属于推荐规则。本项目按用户要求取消示例中的下划线，保留类型识别能力；这是 SSLM 的项目规则，不宣称 Unreal 强制如此。[Epic 资产命名建议](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)

归属缩写：`GP` 为平台中立内容，`Moba` 为通用竞技内容，`DBA` 为项目内容；展示名称继续使用 SSLM。`UI`、`VFX`、`SFX`、`PCG`、`LOD` 等既有缩写统一大写。插件身份必须和资源所在的真实所有者相符，不能靠名称伪装层次。

| 类型 | 前缀 | 新名称示例 |
|---|---|---|
| 地图 / 关卡 | L | LDBAPCGDemo |
| 普通蓝图 | BP | BPDBATreePreview |
| Widget 蓝图 | WBP | WBPDBAPluginPCG |
| 编辑器工具界面 | EUW | EUWDBAAssetInspector |
| 静态网格 | SM | SMDBATreeWillowA |
| 骨骼网格 / 骨架 | SK / SKEL | SKDBAVillageWolf / SKELDBAVillageWolf |
| 动画蓝图 / 序列 / 蒙太奇 | ABP / A / AM | ABPDBAWolf / ADBAWolfIdle / AMDBAWolfAttack |
| 材质 / 实例 / 函数 | M / MI / MF | MDBARock / MIDBARockWet / MFGPSlopeMask |
| 材质参数集合 | MPC | MPCGPSurfaceGlobal |
| 纹理 | T | TDBAWillowBaseColor / TDBAWillowNormal / TDBAWillowORM |
| Niagara 系统 / 发射器 | NS / NE | NSDBAFireballFlight / NEDBAFireEmbers |
| PCG 图 / 植被类型 | PCG / FT | PCGDBAHillsideForest / FTDBAWillow |
| 定义资产 / 数据表 | DA / DT | DADBAForestPreset / DTDBAAssetCatalog |
| 音效 / 声音合成 | S / MS | SDBAForestWind / MSDBAForestAmbience |
| 文档 / 工具脚本 | 语义名称 | NamingStandard.md / AuditPluginResources.py |

上表按项目已有类型语义统一；不为对齐外部风格把已有 NS 改成另一套前缀。ORM 必须在资源合同中明确 R=AO、G=Roughness、B=Metallic，不能只靠名称猜通道。LOD 通常保存在同一个网格资产中，只有独立源模型确有需要时才在源文件名加 LOD0 等后缀。

同主体共享语义词根，例如 `SMDBATreeWillowA`、`MIDBATreeWillowLeaves`、`TDBATreeWillowBaseColor`。A/B 或 01/02 只表达实际形态变体。活动资源不使用 Final、New、Copy、V1/V2/V3、Review 等词形成并行实现；开发入口以 Demo 表达职责。日期用于审核交付目录及证据，不加在当前正式资产叶子名上。建议新资产叶子名尽量短于 64 字符，这是可读性目标，真实路径限制仍由现有 Cook 门禁验证。

## 必须登记的兼容例外

1. Unreal、编译器和工具发现规定的名称、扩展链保持有效，例如 `AGENTS.md`、`.uplugin`、`.uproject`、`.Build.cs`、`.Target.cs`、`.generated.h`、`CMakeLists.txt`。Go 的 `_test.go`、平台构建后缀等属于工具链规则，不为视觉整齐破坏发现和编译。
2. 已发布插件、模块、反射类型、PrimaryAssetId、GameplayTag、协议字段及枚举号不是普通文件名。现有 `DBAFoliagePack_Core` 等挂载身份暂保留；若另行批准身份迁移，必须同时完成描述符、依赖、注册、全部软硬引用和发布兼容验证。
3. 第三方原始交付与历史证据保留来源身份，纳入例外清单。面向项目的分类、显示名称和新适配资产遵守新规范，不复制供应商网格来凑名称。
4. 材质参数、骨骼挂点和序列化字段即使包含下划线，也不能按文件重命名规则直接改动。它们分别遵守现有调用合同。

## 现有文件如何统一

`RenameCandidates.csv` 是当前二进制资源的候选审查清单，不是可直接执行的批量改名脚本。它列出旧文件、去分隔符候选、原包路径、历史/供应商保留、Redirector 处理及冲突标记。最终名称还须根据真实资产类型和主体用途审核；不能把简单删下划线当作完整语义命名。

已发现 `kachujin_MAT.uasset` 与 `kachujin_MAT_.uasset` 会收敛为同名。先检查两份真实材质的用途、引用和来源，再按语义定名；禁止自动覆盖、随意加数字或合并不同材质。

迁移顺序：冲突与来源检查 → 入边/软引用/作者脚本清单 → 工作区内备份 → Monolith 调用引擎 RenameAssets → 保存真实引用者 → 更新唯一清单与代码路径 → 冷重载 → Cook/Stage 与容器核对。重定向对象只在引用收敛后通过引擎处理，不手工删除。[Epic Redirector 说明](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-redirectors-in-unreal-engine)

本次先统一本目录全部新文件，并为 70 个插件和 8 个资源审核场景提供无下划线的建议关卡/页面名。已有资产未被重命名，正式场景与已发布身份未改动。

本轮布局文件统一使用 `<插件或资源职责>Layout.svg`，对应 PNG 存于 `Previews`，不使用 `Screenshots` 误示为引擎截图。实际改名及文件哈希见 `NamingChanges.json`；索引和浏览页同步更新。
