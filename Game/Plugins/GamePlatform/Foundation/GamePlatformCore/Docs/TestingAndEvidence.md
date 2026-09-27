# GamePlatformCore（游戏平台核心）测试与验证证据

> 日期：2026-09-27。
> 本文记录本次 Core 完善后的真实验证结果；未执行项明确标记。

## 1. 原生生产算法测试

入口：`Game/Plugins/GamePlatform/Foundation/GamePlatformCore/Tests/RunNativeTests.ps1`。

实际工具链：CMake 3.31.6-msvc6、Visual Studio 2022 BuildTools、MSVC 19.44.35228、Windows SDK 10.0.26100.0、x64。

场景共13项：identity_valid、identity_invalid、identity_limits、identity_equality、identity_alias、error_code_contract、version_valid、version_invalid、version_order、version_range_contract、wide_characters、result_states、result_diagnostics。

结果：

```text
Debug   13/13 Passed
Release 13/13 Passed
```

## 2. UE5.8 Editor（编辑器）模块构建

使用锁定引擎 `D:\UnrealEngine-5.8.0-release\Engine\Build\BatchFiles\Build.bat`，目标 `DivineBeastsArenaEditor Win64 Development -Module=GamePlatformCore`。

结果：引擎自带 .NET 10 被正确使用；新增 ErrorCode / VersionRange 源码编译成功；`UnrealEditor-GamePlatformCore.lib/.dll` 链接成功；Result: Succeeded；ExitCode 0。

## 3. UE5.8 Client（客户端）模块构建

目标：`DivineBeastsArenaClient Win64 Development -Module=GamePlatformCore`。

结果：UHT processed DivineBeastsArenaClient；新增反射类型及 Core 源码编译成功；Result: Succeeded；ExitCode 0。

## 4. UE5.8 Server（专用服务器）模块构建

目标：`DivineBeastsArenaServer Win64 Development -Module=GamePlatformCore`。

结果：UHT processed DivineBeastsArenaServer；Core 没有引入 ClientOnly 依赖；Result: Succeeded；ExitCode 0。

## 5. 当前未执行

- `GamePlatform.Core.*` UE Automation（虚幻自动化测试）的实际运行。本轮已尝试启动，但锁定源码引擎当前未生成 `UnrealEditor.exe` / `UnrealEditor-Cmd.exe`，因此无法进入自动化运行器；这不是测试用例失败。
- 全工程 Editor/Client/Server 完整构建。
- Cook / Stage（烘焙/暂存）。
- Online / Session（在线/会话）等上层功能联调。

这些不是 GamePlatformCore 单模块编译通过可以替代的证据。

## 6. 工具链说明

直接调用 `UnrealBuildTool.exe` 时系统默认 .NET 8 不满足这份 UE5.8 UBT 需要的 .NET 10；正确入口是 UE 的 `Build.bat`，其会使用引擎自带 `Engine/Binaries/ThirdParty/DotNet/10.0/win-x64`。后续 UE 构建统一使用锁定引擎的 BatchFiles（批处理构建入口）。

## 7. 验收状态

```text
G0 Architecture（架构）        Passed
G1 Compile（编译）             Passed：Core Editor/Client/Server模块
G2 Functional（功能）          Partial：Native通过，UE Automation未运行
G3 Integration（集成）         Not claimed
G4 Content & Manual Review     不适用/未声明：Core无内容资产
G5 Production（生产）          未声明：未做全工程Release/Cook/长稳
```