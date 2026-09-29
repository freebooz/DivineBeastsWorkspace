# GamePlatformSave 人工审核说明

版本：0.2.0｜2026-09-29

## 1. 审核重点

人工 Review（审核）时优先回答以下问题，而不是只检查代码是否能编译。

### 架构

- 是否仍只有 `GamePlatformSaveClient — ClientOnly`？
- 是否出现 `GamePlatformSave → MobaCommon/DivineBeasts` 反向依赖？
- 是否直接依赖 Online、Session、Inventory、Progression、Equipment、UI、Settings？
- 是否把本地 Save 演变成服务器业务数据库？

### 数据权威

发现以下字段或语义应立即阻断：

```text
金币
货币
装备真源
库存真源
等级
经验
任务奖励
比赛结果
排位分
权益真源
服务器世界状态
AccessToken
RefreshToken
Password
```

### 生命周期

- 子系统是否 GameInstance 作用域？
- 是否错误持有 World/Pawn/PlayerController 作为长期真源？
- GameInstance 销毁是否取消登记中的回调？
- 后台任务是否通过 WeakObjectPtr/Generation 防止旧实例回调？

### IO

- 是否所有磁盘操作仍在后台线程？
- 是否存在 Tick/Ticker 自动轮询？
- 是否仍为同目录临时文件 + Flush + Move 替换？
- 备份失败时是否会保护旧主档？
- Load 是否先主档后备份？

### 版本

- Envelope FormatVersion 与 Record SchemaVersion 是否继续分离？
- 新 Schema 是否 Fail Closed？
- 迁移是否由 Namespace Provider 在游戏线程执行？
- 迁移函数是否确定性且不访问网络／阻塞磁盘？

### 安全

- 原始 ProfileKey 是否出现在文件名或诊断日志？
- CRC 是否被错误描述为防篡改认证？
- 是否误加入“加密”宣传但没有真实密钥管理？
- 是否允许本地文件直接改变服务器权威结果？

## 2. 组件清单

| 组件 | 中文说明 | 主要职责 | 使用方 |
| --- | --- | --- | --- |
| `IGamePlatformSaveService` | 平台本地存档服务 | Provider、异步Load/Save/Delete、诊断 | 客户端组合层与领域插件 |
| `IGamePlatformSaveProvider` | 存档命名空间提供者 | 当前Schema和迁移 | MobaCommon/DivineBeasts或其他上层领域 |
| `FGamePlatformSaveKey` | 本地存档逻辑键 | Namespace/ProfileKey/SlotName | 所有调用方 |
| `FGamePlatformSaveRecord` | 本地存档记录 | SchemaVersion + Payload | 所有调用方 |
| `FGamePlatformSavePolicy` | 存档纯值策略 | 校验、SHA-1、Envelope、CRC | 插件内部 |
| `FGamePlatformSaveStorage` | 文件存储适配 | 临时写、Flush、备份、替换、删除 | 插件内部 |
| `UGamePlatformSaveSubsystem` | 游戏实例存档执行器 | 并发门禁、后台IO、迁移、回调、生命周期 | 服务门面 |
| `GamePlatformSavePolicyTests` | 存档策略测试 | 格式、CRC、容量、Key | 自动化验证 |
| `TestGamePlatformSaveArchitecture.ps1` | 静态架构门禁 | 端侧、依赖、结构、关键证据 | CI/人工审查 |

## 3. 调用方审核建议

项目层注册 Provider 时，应特别检查：

1. Namespace 是否稳定且通用；
2. ProfileKey 是否脱敏且非秘密；
3. Payload 是否完全非权威；
4. SchemaVersion 是否有明确迁移策略；
5. 是否错误保存可由服务器决定的结果；
6. 是否对回调捕获的 UObject 做弱引用保护。

## 4. 不应纳入本插件的功能

以下需求出现时，应回到对应领域，而不是继续扩张 Save：

- 服务器角色数据持久化 → Go 后端玩家数据服务；
- 背包／装备／成长 → 对应 PlayerServices + 后端；
- 匹配／跨服／重连 → GamePlatformSession；
- 登录和 Profile API → GamePlatformOnline；
- 设备设置 → GamePlatformSettings；
- 配置资产 → GamePlatformData/Definition；
- 运营动态数据 → GamePlatformLiveOps。

## 5. 发布前人工验收

只有在以下证据均存在时，才能将插件描述为 Production Ready（生产就绪）：

```text
[ ] 架构门禁通过
[ ] Editor/Client 编译通过
[ ] Server 不链接客户端Save模块
[ ] GamePlatform.Save.* Automation实际执行通过
[ ] 主档损坏→备份恢复验证
[ ] 同Key并发验证
[ ] GameInstance销毁取消验证
[ ] 版本迁移验证
[ ] 低磁盘空间/只读目录失败语义验证
[ ] PC真实运行验证
[ ] 目标移动平台文件系统验证（若发布移动端）
```
