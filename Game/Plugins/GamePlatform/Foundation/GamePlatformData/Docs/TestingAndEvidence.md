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