# 配置、构建与运行

配置来自显式 C++ 规格，不假设插件 Default.ini 自动合并。没有添加未消费的空 ini 或 Settings 类。默认操作60秒、任务30秒、权重1；时间用真实单调时钟。最多256任务、单任务最多64个直接依赖、单实例最多128个状态订阅和128个自定义工厂，DAG启动后冻结且同一实例只允许一个未释放操作。未来需要项目调参时由组合根读取项目配置再传规格，不引入客户端密钥。

从仓库根运行，UE_ROOT 指向实际 UE5.8 根，不写入个人目录。2026-09-27 本轮实际验证使用 `D:/UnrealEngine-5.8.0-release`、引擎自带 .NET 10、VS2022 BuildTools / MSVC 19.44.35228、Windows SDK 10.0.26100.0 和 CMake 3.31.6-msvc6。原生 Debug/Release 以及 Editor/Client/Server 模块构建均取得退出码0。

```powershell
$env:UE_ROOT = '<本机UE5.8根目录>'
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Validation/VerifyLoading.ps1 -NativeTests -CMake 'C:/Program Files/CMake/bin/cmake.exe'
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Validation/VerifyLoading.ps1 -Targets Editor -EngineRoot $env:UE_ROOT
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Validation/VerifyLoading.ps1 -Targets Client -EngineRoot $env:UE_ROOT
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Validation/VerifyLoading.ps1 -Targets Server -EngineRoot $env:UE_ROOT
```

VerifyLoading默认进程超时180秒，可用TimeoutSeconds调整；只终止本次启动且身份匹配的进程树。每次写新`Saved/Validation/GamePlatformLoading/<RunId>`，拒绝复用证据目录。原生构建/测试逐项退出0才通过；外部失败码保留。即使所选原生检查通过也返回2，表示完整游戏验收仍缺项。

若正式编译前置修复并已具备PowerShell7，复用现有工程入口：

```powershell
pwsh -File Build/Game/BuildFoundation.ps1 -Editor -Client -Server -EngineRoot $env:UE_ROOT
pwsh -File Tests/Integration/Loading/TestLoadingFoundation.ps1 -Execute -EngineRoot $env:UE_ROOT
pwsh -File Build/Game/RunFoundation.ps1 -Start -FoundationStandalone -Target Editor -Phase FullM0 -EngineRoot $env:UE_ROOT
pwsh -File Build/Game/CookFoundation.ps1 -Cook -Target Client -EngineRoot $env:UE_ROOT
pwsh -File Build/Game/CookFoundation.ps1 -Cook -Target Server -EngineRoot $env:UE_ROOT
```

真实资产由`Tools/AssetTools/CreateFoundationAssets.py`在编译后的编辑器内按其Maps/Probe/Flow阶段生成，使用它的现行参数及防覆盖机制，不拷贝文本伪造包。两份主资产身份为`GamePlatformDefinition:foundation.probe@1`、`GamePlatformDefinition:foundation.flow@1`，世界包为`/Game/Development/Foundation/Maps/L_FoundationSandbox`。缺资产的Data自动化应失败，不跳过后报告成功。

当前源码引擎目录没有可启动的 `UnrealEditor.exe` / `UnrealEditor-Cmd.exe`，因此本轮只能取得 UHT/模块编译证据，不能实际运行 `GamePlatform.Loading.*` UE Automation。后续生成真实资产和执行 Automation 前必须先完成 Editor 可执行程序构建；不得用模块 DLL 编译成功冒充运行测试成功。

上述运行入口是已有Foundation冒烟，不能仅凭Ready日志替代本插件自动化状态断言，也不证明三维交互。自动化脚本只接受本轮导出报告的两个精确测试身份；NullRHI不证明可见性。双PIE须人工开启两个实例验证A取消不影响B；当前未执行。

Cook使用既有`FoundationStandalone`显式开发配置及独立输出目录；正式Shipping必须另做干净Cook确认开发资产未意外带入。Server源码静态无UI/Session客户端依赖，不等同产物审计通过。FoundationSessionLoading前置未满足，没有提供返回假成功的会话脚本。
