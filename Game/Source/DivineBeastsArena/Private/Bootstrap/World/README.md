# Foundation World 项目开发装配

此目录只负责正式主工程的显式开发验证，不是第二套平台 World 或 Flow，不修改默认地图。`UDBAFoundationWorldBootstrap` 是私有、Transient 游戏实例子系统；Shipping、Commandlet、没有 `-FoundationWorld` 时不创建。模块依赖和插件启用由主工程装配任务维护。

## 参数与真实依赖

- `-FoundationWorld`：对当前 Game/PIE 世界运行真实 Data/World/Loading 装配，不自动切图。
- `-FoundationWorldDefinition=GamePlatformDefinition:foundation.world@1`：必须是 PrimaryAssetId，不是包路径；未提供时使用该开发身份，但缺失资产始终失败。
- `-FoundationRunId=<D格式非零GUID>`：脚本证据身份；未指定时生成本次身份，非法输入拒绝。
- `-FoundationWorldScenario=Foundation`：当前仅支持 Foundation；Partition 直接拒绝，不声称分区验收。
- `-FoundationWorldExercise`：显式开启 Sandbox → Bootstrap → Sandbox 自动验证。只在已安装的 Tick 观察后通过项目层 `UGameplayStatics::OpenLevel` 发送；不调用旧 Flow，不增加平台 Travel API。
- `-FoundationWorldServerRole=OpenWorld|Village|MainArena`：仅 Dedicated 的显式开发初始化使用；Dedicated 禁止 Exercise 自动玩家 Travel。

不要附加 `-FoundationStandalone`：它属于旧流程开关，不是本装配的前提。Client/Listen 在 Session 公开可信上下文缺失时拒绝，不能用开发参数伪造 Session。开发 BuildVersion=1.0.0 仅描述本项目夹具，不代表 UE 版本或后端协商。

Data 扫描、AssetManager 配置、World/Data/Loading 模块及真实地图必须由主工程正确装配。这里既不修配置，也不自行 LoadObject 替代 Data。主工程与原生资产尚未经过本轮 UE 编译/生成/运行；三个空描述文件阻断仍保留。

## 资产与边界

`Tools/AssetTools/CreateWorldAssets.py` 在独占 UE 编辑器进程中执行，要求 `-unattended -ScriptErrorsAreFatal -ExecutePythonScript=<绝对脚本路径>`，不要使用 `-run=PythonScript`。先执行既有 `CreateFoundationAssets.py --phase Maps`，新脚本不会修改它或生成替代地图。

| 包路径（均位于 /Game/Development/Foundation/Definitions） | LogicalId | 类型 |
| --- | --- | --- |
| DA_FoundationRegionA | foundation.region_a@1 | GamePlatformRegionDefinition |
| DA_FoundationRegionB | foundation.region_b@1 | GamePlatformRegionDefinition |
| DA_FoundationWorld | foundation.world@1 | GamePlatformWorldDefinition |

World.MapIdentity 是 `/Game/Development/Foundation/Maps/L_FoundationSandbox.L_FoundationSandbox`；Regions 与 RequiredDefinitions 都声明上述两个 Region 的 `GamePlatformDefinition:` 身份。SchemaVersion、ContentRevision 初值为1，ReadinessTimeoutSeconds=60，DefaultExperienceId/ParentRegionId 默认空；RegionTypeTag=Development，AxisAlignedBox/AlwaysRegistered。合法自定义超时、修订和区域语义只报告差异，不重写。

原生资产保存、注册表身份、重载与全 Content 原生文件差异均检查；已存在包只读验证，孤立 sidecar/空包/错误类型拒绝，不覆盖、不删除、不回滚他人文件。报告前缀 `WORLD_ASSETS_REPORT `，必须同时检查 JSON.exit_code 和实际 UE 进程退出码。脚本不产生 `.uasset` 文本占位。

运行时先经 Data 读取 World 与两个 Region 定义，身份使用真实 `LogicalId`。两枚非复制 Transient AActor 只作为项目 Provider 弱所有者：按 World.Regions 顺序绑定两个轴对齐盒，范围分别为厘米 `(-1900,-1900,-200)..(-10,1900,1000)`、`(10,-1900,-200)..(1900,1900,1000)`。它们是项目开发夹具，不宣称与 World Partition cell 对应。

## 就绪、事件与释放

注册同 Provider 第二次必须返回 `DuplicateRegionId`。每个盒中心的 QueryRegion 必须匹配其真实定义身份。以本次 ProviderActors[0] 作为显式观察者及弱订阅所有者，依次提交中心A、中心B、盒外 `(0,0,2000)`；下一轮真实回调必须按顺序得到进入A、离开A、进入B、离开B。没有回调只会等待/超时，不能通过固定延迟获得成功。

全部区域事件完成后，通过公开 `RegisterTaskFactory("WorldReadiness", []{ return GamePlatformWorldServices::CreateReadinessTask(); }, OutResult)` 注册真实工厂并创建必需任务。只持有自己操作句柄；Busy 时等待截止时间，不取消他人操作，不抢占其他注册。Ready 同时检查 Loading 自己句柄与 World 即时快照。

切图先失效旧尝试，释放本 Loading 操作，再撤工厂、区域订阅/Provider、销毁本 Actor、释放本 Data 租约；撤销 Busy 保留句柄重试。World 服务独立持有的资源由 World 生命周期清理，不以本装配租约释放冒充服务停止。实例关闭无法完整撤销时记录明确警告，不伪发 CleanupComplete。

Exercise 的 Travel URL 携带每实例独占 `FoundationWorldOperation=<GUID>`；同 GI、实际地图、URL操作身份和 BeginPlay 全匹配才推进。新 Sandbox 使用新 ContextGeneration，并实际调用 UpdateObserver 提交旧代次，必须得到 `ObserverScopeMismatch`。最终成功要求两轮真实屏障，不因 OpenLevel 返回而宣称到达。

## Erdos 日志协议（13事件）

每个正式事件为单行：

`LogDBAWorld: Display: WorldValidation RunId=<小写D GUID> ProcessId=<真实PID> Scenario=Foundation Event=<事件> Generation=<小写D GUID>`

顺序锁定为：Initialized → DefinitionLoaded → MapMatched → RegionsReady → WorldReady → ReturnedToBootstrap → CleanupComplete → Initialized → DefinitionLoaded → MapMatched → RegionsReady → Reentered → Completed。

前7项旧代次，后6项为不同的新代次。首次 DefinitionLoaded/MapMatched 指本装配已读取并验证的真实 Data 定义与当前地图，最终 WorldReady 还要求 World 服务和 Loading 实际屏障。CleanupComplete 在真实 Bootstrap 到达后报告此前已完成的本装配清理，不等于全引擎资源审计。

仅显式 Exercise 完成第二轮后发 `WorldValidationComplete RunId=<小写D GUID>`。区域回调、重复注册拒绝、旧代次拒绝与旧回调丢弃使用 `FoundationWorld` 诊断前缀，不占用上述13事件。无 Exercise 不自动 Travel、不发最终完成行。

## 已执行与未执行

离线脚本使用测试内存适配器，验证的是真实生成/不覆盖/字段/审计算法；不是 UE 资产证据。纯 C++ Exercise 决策由运行时与 native 测试共用；不是 UE Travel 证据。

从工作空间根运行（所有产物在 Saved，不在插件或源码旁）：

```powershell
python -B -m unittest discover -s Tests/Integration/World -p test_create_world_assets.py -v
cmake -S Game/Source/DivineBeastsArena/Private/Bootstrap/World/Tests -B Saved/Validation/GamePlatformWorld/BootstrapNative -G "Visual Studio 17 2022" -A x64
cmake --build Saved/Validation/GamePlatformWorld/BootstrapNative --config Debug
ctest --test-dir Saved/Validation/GamePlatformWorld/BootstrapNative -C Debug -V
```

红/绿日志位于 `Saved/Validation/GamePlatformWorld/Assets`。最初 MSVC 中文源编码配置导致构建失败，已补 `/utf-8`；随后观察到“策略未实现”的实际红测试，并增加“释放后换了其他实例不得发旧Travel”的红回归，再修实现。不存在 UE 编译/真实区域回调/真实往返/资产生成/网络/Partition/MultiPIE/Cook/Stage 的通过声明；这些均未执行。

本轮实际结果：World资产离线12项通过；未修改的Foundation资产脚本48项回归通过；native Debug/Release分别1个CTest通过，每次执行18项决策检查。后者不代表18个独立CTest。`exercise-red.log` 是首次编码构建失败记录，`exercise-red-feature.log`、`exercise-red-foreign-world.log` 才是实际执行的预期红回归；绿记录为 `exercise-green-final.log`、`exercise-release.log`，资产结果为 `green-final.log`、`foundation-regression.log`。
