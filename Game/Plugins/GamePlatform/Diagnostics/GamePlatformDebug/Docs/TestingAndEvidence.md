# TestingAndEvidence（测试与证据）

已执行证据：
- Tests/Architecture/Test-PluginLayers.ps1（三层插件结构测试）：通过。
- 结果：859 checks（检查项）、9 matrix cases（层矩阵用例）、44 plugin descriptors（插件描述）、76 modules（模块），0 failures（失败）。
- 该测试自身明确 ueBuildVerified=false、cookVerified=false（UE构建/烘焙未验证）。
- Build/Validation/VerifyDebug.ps1（调试静态验证）：通过，77 checks（检查项）、0 failures（失败）；其中再次执行上述 859 项架构检查。
- Build/Validation/VerifyDebugShipping.ps1（正式发布调试验证）：staticSourceGate（静态源码门禁）通过；由于没有真实 Shipping Client/Server packaged artifacts（正式客户端/服务器打包产物），总体状态保持“未执行”。

源码自动化测试：
GamePlatformDebugCoreTests.cpp（调试核心测试）覆盖 Sensitive Filter（敏感过滤）、Snapshot Bounds（快照边界）、Provider Registry（状态提供者注册）、Command Registry（命令注册与重复拒绝）。

新增静态验证入口：
Build/Validation/VerifyDebug.ps1（调试静态验证）与 VerifyDebugShipping.ps1（正式发布调试裁剪验证）。

运行集成入口：
Tests/Integration/Debug/TestDebugGameplay.ps1、TestDebugNetwork.ps1、TestDebugDedicatedServer.ps1、TestDebugShipping.ps1。

已检查当前 Runner（运行器）的 PATH（环境路径）、UE_ROOT/UNREAL_ENGINE_ROOT/UE5_ROOT（常用引擎根目录环境变量）以及常见 UE_5.8 安装位置，未发现 UnrealEditor-Cmd.exe（虚幻命令行编辑器）或 Build.bat（UE构建脚本）。TestDebugGameplay/TestDebugNetwork/TestDebugDedicatedServer（玩法/网络/专用服务器测试入口）已进行入口冒烟，均正确返回“未执行”；因此 Development/Test/Shipping Build、Cook、PIE、Dedicated Server + 2 Clients（开发/测试/正式构建、烘焙、编辑器运行、专用服务器+双客户端）、Networking Insights、Unreal Insights 的真实运行状态保持“未执行”，直到具备 UE5.8 工具链后真实完成。