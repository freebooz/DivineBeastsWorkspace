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
