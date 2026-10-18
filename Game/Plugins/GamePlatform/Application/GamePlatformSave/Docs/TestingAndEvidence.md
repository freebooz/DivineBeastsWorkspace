# GamePlatformSave 测试与证据

版本：0.2.0｜2026-09-29

本文件严格区分“代码已实现”“静态门禁通过”“已编译”“UE Automation 已运行”“真实故障恢复已验证”。

## 1. 自动化源码

模块内 UE Automation（自动化测试）：

```text
Source/GamePlatformSaveClient/Private/Tests/GamePlatformSavePolicyTests.cpp
```

当前覆盖：

- 合法 Key；
- 空 Key 拒绝；
- Envelope 编码／解码往返；
- CRC Payload 损坏检测；
- 超大 Payload 拒绝。

测试名称前缀：

```text
GamePlatform.Save.*
```

## 2. 静态架构门禁

入口：

```text
Tests/Scripts/TestGamePlatformSaveArchitecture.ps1
```

检查：

- 只有 `GamePlatformSaveClient` 一个模块；
- ClientOnly；
- TargetAllowList 只含 Client/Editor；
- `CanContainContent=false`；
- 插件依赖只允许 GamePlatformCore；
- Build.cs 必需依赖；
- 禁止 Online/Session/Inventory/Equipment/Progression/UI/Settings 等横向业务依赖；
- Dedicated Server / Commandlet 门禁；
- 无 Tick/Ticker；
- 异步 IO + Game Thread 回调证据；
- `.tmp`、`.bak`、`Flush(true)`、Move 替换证据；
- CRC 与 SchemaVersion 证据；
- 正式文档完整性。

### RED 基线

2026-09-29 首次修正 PowerShell UTF-8 BOM 编码后真实执行，退出码 1，共发现 **21 项**架构缺口，包括：

- `CanContainContent` 错误；
- 缺少 Client/Editor `TargetAllowList`；
- 插件级依赖未收敛到 `GamePlatformCore`；
- 缺少 Service/Provider/Types；
- 缺少 Policy/Storage/Subsystem/Service；
- 缺少核心插件文档；
- Build.cs 缺少 CoreUObject/Engine/GamePlatformCore。

核心实现落地后再次执行，退出码 1，只剩 **5 项**文档缺口：

```text
Architecture.md
API.md
PerformanceAndSecurity.md
TestingAndEvidence.md
ManualReview.md
```

补齐文档后第三次执行，退出码 0，输出：

```text
GamePlatformSave架构门禁通过：ClientOnly、低耦合、异步IO、版本化封装、原子替换、备份恢复与中文文档边界完整。
```

以上 RED → GREEN 过程证明门禁能够实际捕获原模块壳与中间状态缺口。

## 3. 本轮验证顺序

实现完成后按以下顺序执行：

1. `TestGamePlatformSaveArchitecture.ps1`；
2. `git diff --check`；
3. 定向插件构建或项目 Editor 构建；
4. Client 构建；
5. `GamePlatform.Save.*` UE Automation；
6. Server 依赖审计，确认 Server 不链接 `GamePlatformSaveClient`；
7. 条件允许时执行真实文件恢复测试。

## 4. 真实文件恢复建议用例

需要补充或人工验证：

- 第一次保存，无旧文件；
- 第二次保存生成 `.bak`；
- 主档 CRC 损坏，备份可恢复；
- 主档和备份同时损坏，Fail Closed；
- 磁盘空间不足；
- 目录只读；
- 保存过程中进程异常退出；
- 多 Key 并发；
- 同 Key 并发拒绝；
- GameInstance 销毁，回调 Cancelled；
- 旧 Schema → 当前 Schema 迁移；
- 新 Schema 被旧客户端拒绝。

## 5. 当前证据状态

以下状态必须以本轮真实命令结果更新：

```text
静态架构门禁：已执行，通过（2026-09-29）
Git diff check：已执行，通过（首次仅发现README末尾空行，修正后复跑退出0）
UE BuildPlugin/Editor构建：已尝试；未取得编译终态。AutomationTool停在初始化阶段，诊断确认同工作区已有DivineBeastsArenaEditor UBT长构建占用构建资源；本次等待任务已主动停止，不能记为编译通过
UE Client 构建：未执行
UE Automation：未执行（当前没有可引用的本轮成功编译证据）
Server依赖：静态门禁确认uplugin仅Client/Editor且模块为ClientOnly；真实Server Target链接验证未执行
真实断电/崩溃恢复：未执行
移动端文件系统验证：未执行
```

本轮 `BuildPlugin` 使用独立目录 `Saved/Validation/GamePlatformSave/BuildPlugin`，没有复用其他插件构建日志。构建在真正进入插件编译前即受既有 UBT 长任务影响，因此该尝试只能记为“环境阻塞／未验证”，不能用于证明 C++/UHT 已通过。

任何未执行项目不得写成“通过”。

## 2026-10-18 M1进程级存档槽位防并发修复

### 问题与实现

旧逻辑的 `BusyStorageIds`（忙碌槽位集合）只属于一个 `UGamePlatformSaveSubsystem`（GameInstance存档子系统），而真正文件路径是进程共享的 `ProjectSavedDir`。旧实例 `Deinitialize`（销毁）会立即清除自己的忙碌集合并向调用方发取消，即使其线程池 IO 仍在执行；新 GameInstance 可以同时写同一主档/备份。客户端编辑器多PIE仍禁止真实存档IO，但单进程销毁重建GameInstance依然存在此窗口。

本轮在私有 `FGamePlatformSaveProcessSlotLease`（进程槽位租约）新增同步受锁的 `TryAcquire`（尝试占用）与引用计数析构归还；`BeginRequest`（请求开始）检查本地门禁后获取进程级租约，保存到本次Pending，并分别传给Save、Load、Delete三种线程池任务。取消本GI只撤销其Pending回调，不能解除任务对磁盘文件的实际占用。下一实例在旧后台任务仍持有租约时会收到`SaveSlotBusy`，不会进行IO。原有16请求/GI上限保留，异槽并发不受影响；不会返回取消等于磁盘操作已经回滚的虚假结论。

新增 `GamePlatform.Save.Storage.ProcessSlotLeaseLifecycle` 原生测试源码：不同GI同槽拒绝、不同槽并行、旧Owner销毁但Worker仍持有时拒绝、最后引用释放后新实例再次获取；仅使用随机字符串身份，不写玩家存档。跨进程客户端共享同一Saved目录仍没有OS锁，本修复不声称覆盖该场景。

### 证据与必须继续验收

- GamePlatformSaveClient 的源码与新增原生测试已写入工作区，保留UCLASS/GENERATED_BODY行号与当前UHT缓存兼容，正式UHT重生成仍须由独占UE构建验证。
- **已执行**：锁定UE5.8/MSVC对当前Unity编译单元执行`/Zs`（C++语法检查），调整陈旧UHT生成宏行号对应后退出码0，含本次源码与测试。这不是最新UHT生成或正式模块链接。
- **已执行**：跨插件运行安全源码合同12/12项通过（新增跨GameInstance槽位租约接线检查）；GamePlatformSave架构静态门禁退出码0；重新扫描67个插件并同步生成工作单，当前一致性核对退出码0。
- **实际尝试但未启动目标构建**：当前`BuildFoundation.ps1`定向Editor存档模块因共享Foundation构建/Cook互斥锁被其他任务持有，返回`NotExecuted/2`（未执行），证据在`Saved/Validation/FoundationM0/73cc9707-b535-410f-b72c-fffa64093888/Build`。这不是UBT编译错误，不能用于宣称通过或失败；不抢占他人的引擎锁。
- **待验证**：UE原生Automation、正式UHT/UBT及独立客户端Build/Cook、不同GameInstance的真实磁盘并发测试与跨进程文件锁策略；不把测试代码存在当成通过。
- 旧后台任务在取消后仍可能完成真实IO，行为应由调用方明确表达为`CancelledCallbackButIOInFlight`（回调已取消但IO可能仍进行）语义；不能假设已经回滚存档。针对这个场景的真正UE测试与多进程回归仍缺。
