# GamePlatformSave 架构设计

版本：0.2.0  
日期：2026-09-29

## 1. 定位

`GamePlatformSave（游戏平台本地存档插件）` 位于 `GamePlatform/Application（游戏平台/应用机制）`，只提供跨游戏可复用的**客户端非权威本地存档基础设施**。

正式依赖方向仍为：

```text
DivineBeasts（神兽联盟项目层）
        ↓
MobaCommon（MOBA通用层）
        ↓
GamePlatform（游戏平台层）
```

`GamePlatformSave` 不认识生肖、MOBA规则、角色业务、服务器角色、后端数据库或项目资源；上层通过稳定接口使用本地存档能力。

## 2. 权威边界

允许保存的数据必须满足：即使文件丢失、损坏、被用户删除或篡改，也不会改变服务器权威结果，并且可以安全回退或从其他真源重新获得。

典型允许内容：

- 教程提示是否已在本机看过；
- 本地 UI 布局草稿；
- 非权威地图标注、筛选状态或工具状态；
- 可以丢失并重新生成的本地缓存。

明确禁止：

- 金币、货币和资产余额；
- Inventory（背包）权威库存；
- Equipment（装备）权威装备状态；
- Progression（成长）等级和经验；
- Quest（任务）权威进度与奖励领取；
- Match（比赛）结果、排位结算、隐藏分；
- Entitlement（权益）真源；
- 账号密码、Token、服务器凭据；
- 世界权威状态、技能冷却、实时 Buff、当前生命值等运行真源。

断线重连不得从本地存档恢复服务器权威状态；Reconnect（重连）仍由 `GamePlatformSession（平台会话插件）` 与服务器准入链负责。

## 3. 模块与端侧

当前只保留一个真实模块：

```text
GamePlatformSaveClient — ClientOnly（仅客户端）
```

原因：该能力是客户端本地磁盘存储，Dedicated Server（专用服务器）不应链接或加载它。当前没有建立独立 Runtime/Server/Editor 空模块。

`GamePlatformSave.uplugin` 限制 `TargetAllowList = Client, Editor`，`CanContainContent=false`。

插件级依赖只允许：

```text
GamePlatformCore（游戏平台核心）
```

模块依赖：

```text
Core
CoreUObject
Engine
GamePlatformCore
```

不依赖 Online、Session、Settings、Inventory、Equipment、Progression、UI、Data、MobaCommon 或 DivineBeasts。

## 4. 核心组件

### 4.1 IGamePlatformSaveService

稳定 C++ 服务门面，GameInstance（游戏实例）作用域。

负责：

- Provider 注册／注销；
- 异步 Save/Load/Delete；
- 同槽并发门禁；
- 运行诊断；
- 生命周期取消和完成回调。

### 4.2 IGamePlatformSaveProvider

上层 Namespace（命名空间）扩展契约。

Provider 只声明：

- 唯一 Namespace；
- 当前 SchemaVersion；
- 旧 Payload 到当前版本的确定性迁移逻辑。

Save 插件不会反向依赖 Provider 所属业务插件。

### 4.3 FGamePlatformSaveKey

稳定逻辑键：

```text
Namespace + ProfileKey + SlotName
```

三者只参与 SHA-1 稳定摘要，不直接作为磁盘文件名。

`ProfileKey` 是调用方提供的**脱敏稳定本地档案键**，用于本地多主体隔离，不是在线身份真源，也不得包含秘密。

建议用法：

- 全局本地数据：调用方使用稳定常量 ProfileKey，例如 `global`；
- 账号本地数据：使用由当前主体生成的稳定脱敏键；
- 角色本地数据：使用项目层生成的账号+角色复合脱敏键；
- 世界本地数据：使用项目层生成的账号+世界复合脱敏键；
- Session 临时状态：不应落盘，应由会话／流程内存状态持有。

GamePlatformSave 本身不理解 Account/Character/World 的业务语义，从而保持跨游戏复用。

### 4.4 FGamePlatformSaveRecord

记录结构：

```text
Key
SchemaVersion
Payload
```

Payload 是调用方序列化后的二进制数据，当前单记录最大 8 MiB。

### 4.5 GamePlatformSavePolicy

纯值策略层，不接触 UObject、网络或业务状态。

职责：

- Key 严格校验；
- Record 严格校验；
- SHA-1 稳定磁盘 ID；
- Envelope 编解码；
- CRC32 损坏检测；
- Payload 长度上限。

### 4.6 GamePlatformSaveStorage

私有文件存储层。

文件根目录：

```text
Saved/GamePlatformSave/
```

磁盘文件名只使用稳定摘要：

```text
<sha1>.gpsav
<sha1>.gpsav.bak
```

Storage 不理解业务 Schema，只负责字节读写。

### 4.7 UGamePlatformSaveSubsystem

GameInstance 级执行器。

- 公共请求在 Game Thread（游戏线程）进入；
- 文件 IO 在 ThreadPool（线程池）执行；
- 完成回到 Game Thread；
- Provider 迁移只在 Game Thread 执行；
- 无 Tick/Ticker；
- 同一稳定 StorageId 同时只允许一个 IO；
- 总并发请求存在固定上限；
- GameInstance 销毁时登记中的请求回调返回 Cancelled；
- 后台系统调用本身不做危险的强制线程终止，旧代次完成结果会被丢弃。

## 5. 两层版本模型

### Envelope FormatVersion

由 GamePlatformSave 自己拥有，用于磁盘封装格式兼容。

当前版本：1。

Envelope 包含：

```text
Magic
FormatVersion
SchemaVersion
PayloadSize
PayloadCRC32
Payload
```

### Record SchemaVersion

由对应 Namespace Provider 拥有。

加载逻辑：

```text
StoredSchema == CurrentSchema
    → 直接返回

StoredSchema < CurrentSchema
    → Provider.MigratePayload

StoredSchema > CurrentSchema
    → Fail Closed，不猜测降级
```

迁移发生在内存中，本轮不会自动覆盖磁盘旧记录。调用方确认迁移后的数据有效后，可显式再次 Save，从而避免加载行为隐式写盘。

## 6. 原子写与备份恢复

保存顺序：

```text
Encode
  ↓
同目录唯一 .tmp.<guid>
  ↓
IFileHandle::Flush(true)
  ↓
旧主档存在：复制到 .bak.tmp.<guid>
  ↓
Move 替换 .bak
  ↓
Move 替换主档
```

备份步骤失败时不会覆盖旧主档。

加载顺序：

```text
主档 Read + Decode + CRC
        ↓失败
.bak Read + Decode + CRC
        ↓成功
返回 bRecoveredFromBackup=true
```

CRC32 仅用于检测随机损坏，不是数字签名，不提供防篡改认证；本地记录本来就不能成为权威数据。

## 7. 生命周期与跨地图

服务挂在 GameInstance，因此：

- Login（登录）→角色选择→Village（新手村）→OpenWorld（开放世界）→MainArena（主竞技场）切图期间服务仍存在；
- 不持有 Pawn、PlayerController 或 World 作为真源；
- 账号切换时上层必须切换到新的 ProfileKey；旧账号文件不会自动作为新账号数据读取；
- 插件不主动读取 Online 身份，避免形成 `Save ↔ Online` 横向依赖。

## 8. 事件与并发

本插件采用命令/完成回调模式，不做每帧轮询。

每个请求返回 `FGamePlatformSaveRequestHandle`，并通过 `FGamePlatformSaveCallback` 返回最终结果。

同一个逻辑 Key 的 Load/Save/Delete 不能并发，避免：

- Load 读到一半被 Save 替换；
- Save 与 Delete 互相竞争；
- 两个 Save 最终顺序不可预测。

不同 Key 可以并发，但全局存在固定并发上限。

## 9. 与其他插件的边界

- `GamePlatformSettings`：设备／应用设置，使用 UGameUserSettings；不走 Save。
- `GamePlatformOnline`：认证、后端 Profile 和 Gateway 请求；不把 Token 交给 Save。
- `GamePlatformSession`：Join/Transfer/Reconnect；不通过 Save 恢复权威会话状态。
- `GamePlatformInventory/Equipment/Progression/Quest/Entitlement`：后端／服务器权威业务；只允许在确有需要时保存纯本地视图草稿或缓存，不能保存真源。
- `DivineBeasts`：可以注册项目 Namespace Provider，但 GamePlatformSave 不反向认识项目类型。

## 10. 当前状态

0.2.0 已从模块壳提升为核心机制实现。静态门禁、UE Automation、Editor/Client 编译和真实掉电恢复证据以 `TestingAndEvidence.md` 为准；未执行项目不得写成已验证。
