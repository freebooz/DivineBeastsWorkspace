# GamePlatformSave（游戏平台本地存档插件）

版本：0.2.0｜2026-09-29

`GamePlatformSave` 是 `GamePlatform/Application（游戏平台/应用机制）` 的跨游戏客户端非权威本地存档基础设施。

当前已经从二期模块壳收敛为：

- `GamePlatformSaveClient — ClientOnly（仅客户端）`；
- `IGamePlatformSaveService（统一存档服务）`；
- `IGamePlatformSaveProvider（命名空间版本/迁移提供者）`；
- 版本化 `FGamePlatformSaveRecord（本地存档记录）`；
- SHA-1 稳定文件身份；
- Envelope FormatVersion + Namespace SchemaVersion 双版本；
- CRC32 损坏检测；
- ThreadPool（线程池）异步文件 IO；
- 同槽并发门禁；
- 同目录临时文件 + `Flush(true)` + Move 替换；
- `.bak` 备份与损坏恢复；
- GameInstance（游戏实例）生命周期取消；
- 无 Tick/Ticker；
- 静态架构门禁与 UE Automation 策略测试。

## 权威边界

只允许保存可丢失、可安全回退或由其他真源重建的本地数据。

禁止保存：金币、装备真源、背包真源、等级/经验、任务奖励、比赛结果、排位数据、权益真源、服务器世界状态、密码、Token 或其他秘密。

本地存档永远不能作为服务器权威事实；断线重连仍由 `GamePlatformSession（平台会话插件）` 与服务器准入负责。

## 使用入口

```cpp
IGamePlatformSaveService* SaveService =
    IGamePlatformSaveService::Get(*GameInstance);
```

上层领域如果需要版本迁移，可实现并注册：

```cpp
IGamePlatformSaveProvider
```

详细调用见 `Docs/API.md`。

## 文档

- `Docs/Architecture.md`：最新架构与数据边界；
- `Docs/API.md`：Provider、Load/Save/Delete 使用方法；
- `Docs/PerformanceAndSecurity.md`：性能、原子替换、安全与防作弊边界；
- `Docs/TestingAndEvidence.md`：真实测试与验证证据；
- `Docs/ManualReview.md`：人工审核清单和组件说明；
- `Docs/审查整改方案与执行计划.md`：本轮历史总结、最新方案与执行计划。

正式三层依赖仍为：

```text
DivineBeasts → MobaCommon → GamePlatform
```

GamePlatformSave 不反向依赖任何上层业务插件。
