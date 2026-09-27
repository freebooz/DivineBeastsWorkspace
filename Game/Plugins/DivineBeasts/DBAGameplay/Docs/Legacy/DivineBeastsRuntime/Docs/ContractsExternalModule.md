# ContractsExternalModule（协议外部模块）

DivineBeastsContracts.Build.cs使用Type = ModuleType.External（外部库模块类型）。它不会被Unreal当作普通源码Runtime模块编译，也没有写入DivineBeastsRuntime.uplugin的Modules数组。

Include来自Shared/Generated/Cpp/Games/DivineBeasts；Library来自Artifacts/Contracts/DivineBeasts/<Platform>/<Architecture>/<Configuration>/<Compiler>/UE5.8/。

Win64使用DivineBeastsContracts.lib；Linux映射libDivineBeastsContracts.a。Build.cs按Target.Platform、Target.Architecture、Target.Configuration及Compiler选择路径。

生成目录或目标静态库缺失时直接BuildException（构建异常）快速失败，不默默禁用。当前Runner没有MSVC，因此真实External静态库构建和UE链接为“未执行”。
