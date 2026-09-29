# D07｜配置与可复现运行

当前没有 `UGamePlatformSessionSettings` 或 `DefaultGamePlatformSession.ini`；Session运行时参数仍由调用请求和Transport注入，不创建第二套配置系统。Backend世界进入和控制面通过既有服务环境变量配置。

- Session公开请求 `TimeoutSeconds` 默认45秒，允许1～120秒；内部使用 `FPlatformTime::Seconds()` 单调时钟形成Deadline，不使用World时间。
- PostgreSQL预留TTLSeconds为1～120秒，绑定租约1～30秒，均不得超过短期授权ExpiresAt。服务器容量为正整数，没有套用竞技最多10人的限制。
- TestSessionBackend.ps1的GoImage默认golang:1.23，PostgresImage默认postgres:17-alpine。本次实际Go为1.23.12，和仓库1.23.0补丁锁不完全相同，不能报告精确锁版本验收。
- 隔离测试库名session_integration，容器名含每次生成的GUID，未发布宿主端口。SESSION_INTEGRATION_ISOLATED=1只启用测试断言，不是业务安全开关。没有可给UE使用的后端地址或服务器密钥示例。
- Session.uplugin默认禁用，模块类型ClientOnly并明确Client/Editor列表；Build.cs对其他目标抛错。DBAClient已经通过公开Session API进行项目层适配，服务器准入仍由GamePlatformServer独立承担。
- Gateway世界进入需要 `GAMESERVERCONTROL_SERVICE_URL`、`GAMESERVERCONTROL_INTERNAL_TOKEN`、`GAME_DEFAULT_REGION`；gRPC生产装配使用 `GAMESERVERCONTROL_GRPC_TARGET`。内部Token只进入Authorization Header，不进入URL和业务响应。

现场UE为5.8.0、CL0，路径来自既有Foundation记录并核对Build.version；没有降低版本。CMake实际4.3.2，原生编译器MSVC19.38.33145。原生编译不等于UE所选工具链或反射编译。本机PATH缺go/protoc/pwsh，脚本兼容Windows PowerShell并使用Docker执行Go。

以下命令从工作空间根执行，不含登录凭据：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Build/Validation/VerifySession.ps1 -NativeTests
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tests/Integration/Session/TestSessionBackend.ps1
```

VerifySession在独立内核通过但完整插件未验证时返回2，任一真实执行失败返回1。不要把2改成0。后端脚本在容器临时副本解析依赖/编译带race的测试程序，不修改仓库go.mod/go.sum；数据迁移只执行于本次新库，finally删除自身容器和自身匿名卷。不会关闭既有五服务或其他项目数据库。

独立CMake命令：

```powershell
cmake -S Game/Plugins/GamePlatform/OnlineServices/GamePlatformSession/Tests -B Saved/Validation/GamePlatformSession/Native -G "Visual Studio 17 2022" -A x64
cmake --build Saved/Validation/GamePlatformSession/Native --config Debug
ctest --test-dir Saved/Validation/GamePlatformSession/Native -C Debug --output-on-failure
```

完整运行顺序应为账号/档案→Online登录→Gateway校验当前角色→ `world-entry` → GameServerControl分配与签票→Session真实Transport→Dedicated Server Admission→四事实Ready。前半段Backend入口已经存在；后半段仍缺可信完整Binding和生产Transport，因此不能给出伪造的“一键跨服成功”命令。缺失项和下一阶段接线位置见DeliveryStatus及《最佳修改方案与执行计划》。

数据库就绪等待45秒、每轮Go测试有60秒总期限；当前容器编译/下载及CMake命令未实现统一外部进程总截止和中断后的完整回收协调，属于脚本剩余项。所有正常已执行测试的容器清理已完成；当前不能把这些内核脚本等同最终UE进程管理入口。
