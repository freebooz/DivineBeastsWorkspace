# PCG编辑器模块阶段断点

> 2026-09-29 补充：GamePlatformPCG 1.0 已形成正式改造基线，详见 `../../Docs/改造方案与执行计划.md（改造方案与执行计划）`。本文件继续记录 0.1.0 Editor（编辑器）模块的真实断点，不再作为后续总体设计的唯一依据。

2026-09-21。按用户转入第九插件的指示提前收口；本目录是已写源码，不是UE验收通过。

## 当前实现与文件

- `GamePlatformPCGEditor.Build.cs`：Editor（编辑器）模块，显式依赖 PCG、Runtime公开接口、UnrealEd、AssetRegistry、DataValidation 和 Projects；DataValidation 只留在编辑器闭包，不反向污染 Runtime/Client/Server。
- `GamePlatformPCG.uplugin（插件描述）`：Editor 模块加载阶段调整为 `Default（默认）`，仍保持 `Type=Editor` 与 `TargetAllowList=[Editor]`。原因是 Commandlet/Validator（命令行工具/校验器）需要在命令类解析与编辑器初始化期间注册反射类型；模块本身仍不自动生成资产，不进入 Client/Server。
- `Private/GamePlatformPCGEditorModule.cpp`：默认模块注册，不自动创建世界或执行生成。
- `Private/Authoring/GamePlatformPCGEditorLibrary.h/.cpp`：反射工具入口；保留0.1.0开发夹具创建，同时新增 `CreateFoundationTemplateAssets（创建基础模板资产）`、Template Contract（模板合同）校验和模板ID查询。全部创建入口先检查目标包，拒绝覆盖已有人工资产；保存失败保留现场并返回错误，不假装事务回滚。
- `Private/Authoring/PCGDevelopmentGraph.h/.cpp`：保留 Legacy（旧版）四节点图生成器；新增使用 UE5.8 原生 `UPCGGraph/AddNodeOfType/AddEdge` 的 M0/M1 Foundation Template Generator（基础模板生成器），设置官方 `bIsTemplate=true`，使用动态默认Graph输入/输出Pin（引脚），不硬编码项目资产。
- `Private/Manifests/PCGSourceFingerprint.h/.cpp`：递归AssetRegistry包依赖与实际包/伴随文件字节、PCG及本插件源码/描述、Build.version与引擎版本参与摘要。脏包、未知依赖和缺源码拒绝；不是仅路径/Seed哈希，也不是密码学签名。
- `Private/Commands/GamePlatformPCGFoundationTemplatesCommandlet.*`：Foundation模板命令行生成器；编译后由 `UnrealEditor-Cmd -run=GamePlatformPCGFoundationTemplates` 显式执行，只写 `/Game/Development/Foundation/PCG/Templates`。
- `Private/Validators/GamePlatformPCGWorldValidator.*`：原生 `UEditorValidatorBase（编辑器验证基类）`；PCG地图必须唯一 WorldDirector（世界编排器），且所有平台PCG放置器必须显式注册。
- `Private/Tests/PCGEditorTests.cpp`：来源摘要、Legacy图及12个M0/M1 Foundation模板内存创建/合同验证的 UE Automation（虚幻自动化测试）源码；尚未实跑。

## 实际能力边界

`CreateDevelopmentAssets`只在未来明确调用时创建以下四个包：

- `/Game/Development/Foundation/PCG/Graphs/PCG_Cosmetic`：装饰原生图。
- `/Game/Development/Foundation/PCG/Graphs/PCG_StaticCollision`：静态碰撞原生图。
- `/Game/Development/Foundation/PCG/Profiles/DA_PCG_Cosmetic`：编辑器静态装饰配置。
- `/Game/Development/Foundation/PCG/Profiles/DA_PCG_StaticCollision`：编辑器静态碰撞配置。

两个配置都为EditorGeneratedStatic，不是运行时装饰入口。网格使用引擎Cube；无地形、坡度、排除体积或任意外部输入支持。固定初始参数只作为开发夹具，区域身份仍需World正式接入。

`InspectProfileSource`只接受已加载且已保存的配置/图/网格，调用runtime公开图检查后返回来源记录。摘要范围为磁盘源包及已列明源码，不承诺覆盖虚拟化外部bulk存储、整个引擎补丁二进制、Cook产物或运行环境配置；不得直接批准生产内容。工具需要独占创作进程，不能与其他写包过程并发。

## 明确未交付与验证

Foundation Template Commandlet（基础模板生成命令行工具）已经通过 `UnrealEditor-Cmd.exe` 实际启动项目，但旧 Editor 模块使用 `PostEngineInit` 加载阶段，日志没有出现 `GamePlatformPCGEditor` 或 Commandlet 注册记录，命令未进入 `Main()`。现已将 Editor 模块调整到 `Default` 加载阶段；需要重新编译 Editor 模块后再次运行。当前仍没有新增 M0/M1 Foundation `.uasset`，也没有创建 Gold Level `.umap`，不能把源码合同存在等价为真实资产已交付。

UE5.8 定向构建已经多次完成 UHT 并进入实际 PCG 编译动作，但受同工作区并行 UBT（虚幻构建工具）任务的 Mutex（互斥锁）、外部停止或工具超时影响，尚未取得本轮 Runtime+Editor 最终编译通过结论。UE Automation、DataValidation 实际调度、模板资产落盘、Gold Level、Client/Server Cook、碰撞/Nav/重开与性能 Profile 均未完成。

下一步顺序固定为：等待同工作区其他 UBT 任务释放 → 定向编译 `GamePlatformPCG + GamePlatformPCGEditor` → 运行 `GamePlatform.PCG` Automation → 运行 Foundation Template Commandlet → 重新验证保存资产 → 再进入最小森林/排除闭环和 M1 Gold Level。HiGen、World Partition、自动桥和 P9 状态不能提前。

运行生成继续由 Runtime 的 `UGamePlatformPCGWorldSubsystem（PCG世界子系统）`维护；Editor 模块不复制运行生命周期。任何清理仍必须先取消并证明原生任务排空，不能复制引擎私有实现或越过 GamePlatformData（平台数据）租约所有权。
