# ConfigurationAndRun（配置与运行）

正式UE工程仍为Game/DivineBeastsArena.uproject（神兽联盟唯一正式UE工程），已启用DivineBeastsRuntime插件。

生成项目核心目录：
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Contracts/Generate-DivineBeastsContracts.ps1

验证干净再生成：
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Contracts/Test-DivineBeastsContractsClean.ps1

构建External静态库：
Build/Contracts/Build-DivineBeastsStaticLibrary.ps1。当前Runner缺少MSVC，因此该步骤实际状态为“未执行”。

UE Client/Server/Editor Build需要真实UE_ROOT和对应工具链。当前Runner没有UnrealEditor-Cmd.exe，不能把源码检查写成UE Build通过。
