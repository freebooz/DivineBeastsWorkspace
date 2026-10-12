# Task 4 独立审查记录

## 结论与边界

2026-10-12，独立审查者在未参与原 Task 4 实现的情况下审阅 task-4-report.md、ImplementationPlan_20261012.md 的 Task 4 要求、Arena 迁移后的头文件/Build.cs、VFX 诊断注册表/模块/世界子系统及两项 Automation 源码。发现下列两项需修复问题；主代理已接受，并在记录本结论之后授权同一审查者仅修复这两项 VFX 缺陷，修补差异由主代理另行复核。

本轮未运行 UnrealEditor、UBT、Cook 或 Automation。真实接口核对来源为 F:\UnrealEngine-5.8.0-release 的源码，静态结论不代表动态测试通过。以下行号对应修补前源码。

## P1：生命周期测试重复实例化世界子系统

- 文件：Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXDiagnosticsTests.cpp，57–64、72、81–84 行。
- 触发：普通 EditorContext、非 commandlet、非 Dedicated Server 执行 WorldSubsystemLifecycle。CreateWorld(Game, false) 已经初始化引擎管理的 VFX 世界子系统，测试又 NewObject/Initialize 一份。
- 影响：每个测试世界登记两次，72 行实际 RegisteredWorlds 为 Before + 4；81 行在 DestroyWorld 前实际为 Before + 2，测试断言失败。手动 Deinitialize 的断言也不能证明引擎世界销毁会注销正确实例。
- 真实引擎证据：Engine/Source/Runtime/Engine/Private/World.cpp，2850（CreateWorld 调 InitializeNewWorld）、2761（调用 InitWorld）、2447（调用 InitializeSubsystems）、10592（SubsystemCollection.Initialize）。VFX 世界子系统 ShouldCreateSubsystem 第 63–70 行允许普通 Game 世界。
- 最小修复：优先获取 World->GetSubsystem 的引擎实例，销毁世界驱动其 Deinitialize，再检查账本；保留强引用用于迟到 Update/重复 Deinitialize 回归。Commandlet 生产门禁拒绝自动创建时，允许仅测试显式创建/初始化并自行 Deinitialize，注明这一路不能证明引擎自动创建；不修改 ShouldCreate 门禁或全局 commandlet 标志。

## P2：生产 Pending 变化没有完整发布到世界快照

- 文件：Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp，475、496、553、568 行；关联调用链 259–265、454、864–868、1456 行。
- 触发一：定义缓存命中后 QueueSelectedResources 将 PendingInstanceCount 从 0 增至 1，但 Play 成功受理后直接返回，未发布计数。
- 触发二：首次定义加载的资源阶段已在 454 行发布 Pending = 1；HandleSelectedResourcesLoaded 成功将其减至 0 后，执行实例成功直接返回，没有发布计数。
- 影响：全局 Pending 可持续少算或多算，直到同一世界下次其他更新/实例结束。其他世界更新只汇总旧的世界快照，不能纠正漏报。此缺口来自既有生产入口，但本轮跨世界汇总正确性仍受其影响。
- 最小修复：核对全部 Pending 增减/复位路径，以集中辅助入口同步变化与快照发布；为真实资源入队、完成、取消边界加入回归，不能仅在测试中赋计数再手动 Update 掩盖缺口。

## 未发现其他确定阻塞的部分

- Arena BackendClient 与 Coordinator 的具体使用者经 Game 全树符号检索均在 GamePlatformArenaServer 模块内；Public 子系统只保留前置声明及 TSharedPtr，项目扩展接口仍为公开导出契约。HTTP/Json 头使用位于 Private，依赖转私有符合当前实际消费者。未观察到要求跨模块访问迁移头的调用方。
- VFX 账本仅持有 FGuid/整数，不持有 UWorld；注销后拒绝旧 ID，世界退出不重置模块峰值；int64 汇总后饱和到非负 int32，饱和后重新按存活世界求和可保留其他世界贡献。
- 模块边界 Reset 与世界 Deinitialize 的早期注销、bClosing/失效 ID 检查保持不同职责；重复关闭不会复活世界记录。
- 新账本、世界标识及测试已有中文职责/生命周期说明；修补将补充生产 Pending 辅助入口及夹具边界注释。

## 已实际执行的只读验证

- Cwebcodex read_files：读取上述报告、计划、Arena/VFX 源码及测试（成功）。
- rg -n：Game 树中 BackendClient/Coordinator 具体符号和 include 路径检索（成功，消费者均在 ArenaServer 模块）。
- rg -n：VFX PendingInstanceCount、UpdateRuntimeDiagnostics、初始化/关闭调用检索；初次附带不存在的 Public/Subsystems 路径返回 exit 2，随后针对真实 Private 路径读取核对完成。
- Python 只读查看真实 UE5.8 World.cpp、WorldSubsystem.cpp、SubsystemCollection.h；确认 CreateWorld 自动初始化调用链与 FSubsystemCollection 默认构造可用（成功）。
- 未执行动态验证；后续修补结果与精确 Automation 名称补记于本文件末尾，统一 UE/Automation 由主代理执行。


## 主代理授权后的限定修补（2026-10-12）

仅修改下列 VFX 私有文件及本审查/原实施报告，未修改 Arena、共享目录规划、执行台账或其他代理文件：

1. Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.h：新增私有 AdjustPendingInstanceCount 与资源诊断回归友元；公开API及生产 ShouldCreateSubsystem 门禁保持不变。
2. Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp：10 处 Pending 增减统一经 AdjustPendingInstanceCount 发布世界快照；int64 中间量后限制到非负 int32。初始化在注册前清零；关闭先注销，末尾只清本地计数，不能复活记录。
3. Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXDiagnosticsTests.cpp：共享夹具优先取得引擎子系统，由 DestroyWorld 注销后断言；仅 Commandlet 自动创建被生产门禁拒绝时显式初始化/关闭。新增真实资源状态转换回归。

### 精确测试名称和范围

- GamePlatform.VFX.Diagnostics.MultiWorldCounts（原有名称，保留）：纯值账本的多世界、退出、迟到更新、峰值和饱和行为。
- GamePlatform.VFX.Diagnostics.WorldSubsystemLifecycle（增强）：普通 Editor 真实引擎实例随世界销毁注销；一个世界销毁保留另一世界。持有强引用后调用迟到 Update/重复 Deinitialize；Commandlet 分支明确仅验证显式生命周期。
- GamePlatform.VFX.Diagnostics.ResourcePendingTransitions（新增）：以真实 UGameInstance/Data 公开服务加载已存在的 UNiagaraComponent UClass 软资源，实际 Play 命中私有缓存后申请资源；检查入队立即发布、生产 ReleaseInstanceResources 取消立即发布、真实异步完成立即发布。完成断言时 Composite 仍活跃，避免 Cleanup/自然结束的另一次发布掩盖缺陷。

资源用例复用既有 ResourceOwnership/DataIntegration 的测试边界：仅在本用例自有的瞬态 Composite 对象和私有缓存中搭建已解析元数据；通过反射设置该对象的 PreloadAssets，不增加生产 setter，不修改共享定义或注册虚构资产。普通软资源确实经 Data 租约加载；此用例不证明主定义资产发现、依赖资产存在或 Niagara 播放。子步骤延迟且测试世界不交给引擎 Tick，在验证资源回执后销毁并清理定时器。已撤销租约的旧回执是明确的拒绝复活输入，不作为资源成功证据。

### 修补后实际验证

已运行的精确命令：

```text
rg -n "PendingInstanceCount|AdjustPendingInstanceCount|GamePlatform.VFX.Diagnostics." Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.cpp Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Subsystems/GamePlatformVFXWorldSubsystem.h Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Tests/GamePlatformVFXDiagnosticsTests.cpp
```

结果：成功，27 条匹配。生产增减位置为 CPP 254、312、341、401、422、467、488、545、560、673 行，统一入口为 1110–1117 行。直接赋值仅为初始化 78 行、退出 158 行和辅助入口 1115 行；头文件 255 行为构造时默认值。

另外实际运行 Cwebcodex run_script（language=python，purpose=validation）的只读源码断言，返回 success=true，四项 PASS：

1. CPP 的 Pending 直接写入限定为两处生命周期复位和一处统一入口。
2. 10 个生产 Pending 差额入口全部使用辅助函数。
3. 三个完整 Automation 名称各登记一次；夹具优先 GetSubsystem，仅 IsRunningCommandlet 分支显式创建。
4. 新资源回归调用生产 Play/资源释放/完成路径，无直接赋 Pending、无全局诊断 Reset、无改动全局 commandlet 标志。

真实 UE5.8 额外只读核对：AutomationTest.h 的 TestEqual 返回 bool；UnrealType.h 提供 const FProperty 对可变 UObject 的 ContainerPtrToValuePtr；SubsystemCollection.h 提供默认构造。尚未执行 C++ 编译或 Automation，以上均不能记为运行通过。

### 待主代理统一验证

```text
Automation RunTests GamePlatform.VFX.Diagnostics.MultiWorldCounts
Automation RunTests GamePlatform.VFX.Diagnostics.WorldSubsystemLifecycle
Automation RunTests GamePlatform.VFX.Diagnostics.ResourcePendingTransitions
```

上述三个运行命令均未在本子任务执行。需由主代理复核修补差异，并在普通 Editor 场景验证自动生命周期；Commandlet 的显式分支不能替代这一证据。真实 Data 异步回执用例有 5 秒测试超时，失败/超时仍由夹具清理自有服务、世界和租约。未运行 UE、UBT 或 Cook，未提交或推送。
