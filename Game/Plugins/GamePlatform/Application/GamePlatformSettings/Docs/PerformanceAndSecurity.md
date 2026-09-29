# GamePlatformSettings 性能、安全与线程边界

## 1. 性能

### 0 Tick

Runtime、Client、Server、Editor 均不注册 Tick/Ticker 或固定轮询。设置变化只由显式命令、Provider 拓扑变化、Reload、异步保存完成等事件触发。

### 查询复杂度

- Registry：TMap SettingId 查询，接近 O(1)。
- Snapshot：TMap SettingId 查询，接近 O(1)。
- SetValue：单层 TMap 更新。
- Apply/Reload：与 Descriptor 数量线性相关，只在低频设置操作执行。
- ChangeSet 发布：O(订阅数)，订阅数受 Project Settings 容量限制。

Gameplay 热路径应读取 Snapshot，不反复解析配置层或访问磁盘。

### IO

- 通用 User 层：只有显式 Save 才触发 `AsyncSaveGameToSlot`。
- 设备显示设置：Stage/Preview/Cancel 不写盘；Confirm/Commit 调用一次原生 SaveSettings。
- Server：只读部署配置，不运行时回写。
- Slider / ValueChanged 只改变 UI 草稿或内存设置，不直接 Flush。

## 2. 安全与健壮性

### Fail Closed

以下场景明确失败：

- 未注册 SettingId。
- 值类型与 Descriptor 不一致。
- NaN/Inf 等非有限 Number。
- 最小／最大值越界。
- 重复 ProviderId／SettingId。
- 配置层与端侧／PersistenceScope 不匹配。
- Profile 比 Runtime 新。
- Migration 缺失、重复或失败。
- Client 设备非法分辨率、窗口模式、帧率或画质等级。
- 超过4096字符的 String 设置。
- 敏感 Descriptor 尝试进入 User／Server 等持久化作用域。
- Server 敏感设置尝试通过 INI／Environment／CommandLine 覆盖。
- User持久化作用域未显式声明Client端侧，或Server持久化／ServerDefault未显式声明Server端侧。
- 两个Server SettingId规范化后映射到同一个 `GP_SETTING_*` 环境变量键。

### 敏感信息

### 用户档案隔离

Client User Profile 不再使用全设备单一固定槽。上层只传入不透明 `UserContextKey`，客户端以 SHA-1 派生槽后缀，实现同设备多账号隔离；原始键不进入 Snapshot、诊断、文件名或普通日志。Logout 空键时禁止 User Profile 保存。该哈希仅用于本地命名隔离，不作为密码学身份认证或安全令牌。

Descriptor.bSensitive=true 时，普通诊断只显示脱敏占位，不输出值；该标记不提供加密。敏感 Descriptor 只能使用 Session 临时作用域。Server 允许存在此类 Session Descriptor，但只有检测到 INI／Environment／CommandLine 的真实覆盖尝试时才 Fail Closed，不能仅因 Descriptor 存在就阻断服务器启动。Settings 不保存账号 token、密码、密钥、角色数据、经济数据，Server 敏感配置必须使用部署秘密机制。

### 迁移安全

Migration 在 User 层副本执行；链中任一步失败都不提交半迁移值，不删除原存档。

## 3. Client / Server 隔离

- Runtime：不依赖 UMG／Slate／EnhancedInput／Niagara／AudioMixer。
- Client：不依赖 Server 模块；普通 PIE 不写真实本地用户档案。
- Server：不依赖 UGameUserSettings／SaveGame／UMG／Slate／输入／音频。
- Editor：不进入 Shipping。
- uplugin TargetAllowList 和模块类型共同约束产物边界。

## 4. 线程安全

公开服务仅游戏线程修改状态。异步 SaveGame 完成回调可以从异步路径到达，但 Runtime 使用弱 UObject 引用并切回 GameThread 后更新 Snapshot。

Runtime 不从后台线程直接操作 UObject，不捕获必须存活的裸 UObject，不允许回调期间同步反入修改。

异步保存期间发生 Provider 拓扑变化时只记录 PendingTopologyReload；只有保存成功且 SavedMutationGeneration 与当前 MutationGeneration 一致时才执行重载。若保存失败或其间有新修改，延迟重载继续保留，避免 Registry 重建丢失未保存值。

## 5. 权威边界

客户端用户设置不参与网络权威。Server 配置只决定被批准的服务器设置项，不替代 GamePlatformServer／DBAServer 的生命周期、准入、容量和比赛权限逻辑。

