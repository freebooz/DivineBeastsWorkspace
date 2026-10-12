# GamePlatformData（游戏平台数据）测试与验证证据

> 日期：2026-09-27。
> 本文记录本次完善后的真实验证结果，并区分 Native（原生测试）、UE模块编译、UE Automation（虚幻自动化）、Cook/Stage（烘焙/暂存）。

## 1. 原生 Demand Ledger（需求账本）

入口：`Game/Plugins/GamePlatform/Foundation/GamePlatformData/Tests/`。

实际工具链：

```text
CMake 3.31.6-msvc6
Visual Studio 2022 BuildTools
MSVC 19.44.35228
Windows SDK 10.0.26100.0
x64
```

结果：

```text
Debug   1/1 Passed
Release 1/1 Passed
```

该原生测试内部覆盖20个需求账本断言，包括多租约 Bundle 并集、重复租约拒绝、跨作用域隔离、共享依赖回滚和空 Bundle 根资产持有。

原生测试不覆盖 UObject、AssetRegistry、Ticker 或真实 AssetManager。

## 2. UE5.8 Editor Runtime 模块

目标：

```text
DivineBeastsArenaEditor Win64 Development
-Module=GamePlatformData
```

结果：Succeeded。

本次新增：

- Definition AssetRegistry 元数据。
- 自依赖校验。
- DataLimits 统一安全上限。
- Diagnostics 累计计数。
- Bundle 数量门禁。

均通过运行时模块编译。

## 3. UE5.8 Editor 模块

目标：

```text
DivineBeastsArenaEditor Win64 Development
-Module=GamePlatformDataEditor
```

首次编译发现两个历史测试使用 UE5.8 已不存在的 `PKG_Transient`，编译失败。

核对 UE5.8 `EPackageFlags` 后，将内存测试包标记改为 `PKG_NewlyCreated`；该标志符合“新建、尚未保存的编辑器内存包”语义。

修复后：

```text
GamePlatformDefinitionValidationTests.cpp 编译成功
GamePlatformRuntimeDependencyTests.cpp 编译成功
GamePlatformDataDefinitionValidator.cpp 编译成功
UnrealEditor-GamePlatformDataEditor.dll 链接成功
Result: Succeeded
ExitCode: 0
```

## 4. UE5.8 Client（客户端）模块

目标：

```text
DivineBeastsArenaClient Win64 Development
-Module=GamePlatformData
```

结果：Succeeded。

这证明 Data Runtime 本轮没有引入 EditorOnly（仅编辑器）依赖。

## 5. UE5.8 Server（专用服务器）模块

目标：

```text
DivineBeastsArenaServer Win64 Development
-Module=GamePlatformData
```

结果：Succeeded。

这只能证明代码端可进入 Server Target（服务器目标）；不能证明内容资产没有 ClientOnly 依赖。Server-safe 仍需真实 AssetRegistry/Cook/Stage 证据。

## 6. UE Automation（虚幻自动化）

现有测试身份包括 Definition、Editor SourceDuplicate、DependencyGraph、DepthLimit、ValidatorDispatch、Ticker调度、RealAssetLeases、RecursiveFailureRollback 等。

本轮未实际运行，因为锁定源码引擎当前没有可启动的 `UnrealEditor.exe` / `UnrealEditor-Cmd.exe`。

另外，`GamePlatform.Data.Runtime.RealAssetLeases` 要求提供真实已保存测试 Definition；当前工程真实 `.uasset` 数量仍为0，因此不能伪造该项通过。

## 7. Cook / Chunk / Server-safe

当前主工程：

```text
PrimaryAssetType = GamePlatformDefinition
Directory = /Game/Development/Foundation/Definitions
ChunkId = -1
CookRule = Unknown
```

因此：

- 尚未形成正式生产扫描目录。
- 尚未锁定 Chunk。
- 尚未通过 Client/Server Cook。
- 尚未通过 Server Asset Safety（服务器资产安全）审计。

本轮没有把这些未验证项写成通过。

## 8. 当前验收状态

```text
G0 Architecture（架构）        待最终全局回归，本插件方向通过
G1 Compile（编译）             Passed：Editor Runtime / Editor / Client / Server
G2 Functional（功能）          Partial：Native通过，UE Automation未运行
G3 Integration（集成）         Partial：真实服务测试源码存在，未实际运行
G4 Content & Manual Review     Not Passed：无真实Definition资产
G5 Production（生产）          Not Passed：无正式Cook/Chunk/Server-safe/长稳
```

## 9. 2026-10-12：模块私有唯一源索引增量

本节对应 `Docs/Implementation/PluginArchitectureOptimization/task-2-report.md`。前八节保留2026-09-27阶段记录，不是本次增量的构建/运行结果。当前实际锁定引擎源码位于 `F:/UnrealEngine-5.8.0-release`；本任务只读核对其接口，不启动编辑器或停止其他任务。新增源码已写入；本子任务没有运行UE构建/Automation，结果待主代理统一登记。

### 新增的精确测试清单

六项都在 `GamePlatformData/Private/Tests`，独立索引行为使用生产 `FDefinitionSourceIndex` 配可控源输入，真实注册表回归额外受 `WITH_EDITOR` 保护。不修改运行模块依赖、反射或Public接口。

| 完整Automation名称 | 输入、断言及边界 |
|---|---|
| `GamePlatform.Data.Index.ColdWarmRepeatedQueries` | 未就绪不扫描；首次建一次；128次暖查询及缺失/非法查询不新增枚举；同路径重复记录不误报两个源 |
| `GamePlatform.Data.Index.IncrementalAssetChanges` | 增删、重复修复、LogicalId变化、重命名、非法标签、非定义同名标签；普通事件保持枚举次数；明确失效按需重建 |
| `GamePlatform.Data.Index.EditorCandidateReplacement` | 同路径候选替换、候选换ID、与别源冲突、非法候选；覆盖只作用于本次查询 |
| `GamePlatform.Data.Index.DiscoveryCancellationAndShutdown` | 后续发现拒绝旧快照；扫描中失效/关闭取消发布；关闭后事件不能复活，显式重启冷建 |
| `GamePlatform.Data.Editor.SourceIndexRegistryEvents` | GUID隔离真实内存定义；AssetCreated/AssetDeleted/AssetUpdateTags/AssetRenamed；公开入口和本地生产实例；64次暖查计数；清理后重建不复活源 |
| `GamePlatform.Data.Editor.SourceIndexRegistryShutdown` | 真实Registry八个委托的所有者检查；取消未发布重建并实际解绑；关闭后真实资产变化不影响旧实例；重启和最终清理 |

真实Registry夹具沿用既有未保存内存包方案，不写 `.uasset`，不改项目资产或AssetManager扫描配置。标签更新调用UE5.8 `AssetUpdateTags(..., FullUpdate)`；完整清理调用 `AssetDeleted`、清除Public/Standalone标记、标记垃圾，并验证测试根无可见资产。非定义及未知类同名标签负例只直接投递给独立索引实例，不广播伪造全局事件。未知类负例只验证拒绝路径，不代表已经验收真实未加载蓝图派生定义或Cooked发现。

### 主代理统一执行入口

新增六项无需外部Definition参数。现有Data完整回归仍需保留 `-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1`；参数缺失或资产不可用不得算通过。现有ResourceLeases、RealAssetLeases、RecursiveFailureRollback、PIE/多实例及调度回归均保留。

以下是待执行命令模板，不是已运行结果；工程路径与现有入口已经核对：

```powershell
& 'F:/UnrealEngine-5.8.0-release/Engine/Build/BatchFiles/Build.bat' DivineBeastsArenaEditor Win64 Development '-Project=E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' -Module=GamePlatformData -WaitMutex
& 'F:/UnrealEngine-5.8.0-release/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/poject/feebooz/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' -unattended -nop4 -NullRHI -nosplash -stdout -FullStdOutLogOutput '-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1' '-ExecCmds=Automation RunTests GamePlatform.Data' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/poject/feebooz/DivineBeastsWorkspace/Game/Saved/Automation/PluginArchitectureOptimization/DataIndex'
```

主代理还需按实际目标构建策略确认Client/Server的Data模块，并在导出中逐项核对六个新测试被发现、完成且Success。现有 `Build/Validation/TestFoundationUnreal.ps1` 可运行Data前缀，但其历史必需名单未包含这六项，不能仅凭旧名单或通过数量视为新索引已验收。

### 本次已执行的轻量核对与剩余边界

- `git diff --check -- Game/Plugins/GamePlatform/Foundation/GamePlatformData` 已实际执行，工具返回成功，无空白错误输出。
- 只读核对锁定UE5.8源码中的八个事件、GameThread广播约束、`IsLoadingAssets / IsGathering`、递归类过滤、真实内存资产更新/重命名与委托所有者查询接口。发现并修正本增量错误的GuardValue包含路径为实际 `Templates/UnrealTemplate.h`。
- 精确修改/接口边界与最终轻量结果见Task 2报告。上面的128/64是测试输入和预期，不是已执行通过数量；不填虚构性能收益。
- 本子任务未运行UE构建、Automation、Cook/Stage、图形、联机或PIE会话。实际未加载蓝图派生定义、运行Cooked注册表及长时间真实异步发现仍需由主代理/后续对应环境补证。
