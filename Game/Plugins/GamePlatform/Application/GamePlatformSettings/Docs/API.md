# GamePlatformSettings 公开 API

## 1. Runtime 服务入口

`IGamePlatformSettingsService::Get(UGameInstance&)`

返回当前 GameInstance 的设置服务；不存在时返回 nullptr，不创建进程全局替身。

主要方法：

- `GetValue(SettingId)`：从已 Apply Snapshot 接近 O(1) 读取。
- `GetDescriptor(SettingId)`：读取稳定 Descriptor 副本。
- `SetValue(SettingId, Layer, Value)`：只改允许的运行时内存层，标记 Dirty。
- `ResetValue(...)` / `ResetCategory(...)`：删除指定层覆盖。
- `Apply(Reason)`：确定性解析、校验、生成 Snapshot、批量广播。
- `Save()`：异步保存 User 层；保存期间 Provider 拓扑变化延后处理，并由 MutationGeneration 防止旧保存回调错误清除新修改。
- `Reload()`：重建 Provider、读取持久层、执行 Migration、重新解析。
- `SwitchUserContext(UserContextKey)`：客户端切换不透明 User Profile 上下文并 Reload；空键表示 Logout。存在待 Apply、脏 User 层或异步 Save 时明确拒绝，不依赖 GamePlatformOnline。
- `GetSnapshot()` / `GetDiagnostics()`：只读状态。
- `Subscribe()` / `Unsubscribe()`：低频批量变化事件。

## 2. Provider API

`IGamePlatformSettingsProvider`

- `GetProviderId()`
- `GetSettingDescriptors()`

上层模块只注册自己的 Descriptor，不修改平台 Registry。

当前正式源码没有非测试生产 Provider；该 API 是扩展合同，不等于已有项目 SettingId 完成迁移。

`IGamePlatformSettingsPersistenceProvider`

- `SupportsRuntime()`
- `Load()`
- `BeginSave()`
- `SetUserContext(UserContextKey)`：持久化适配器接收不透明用户上下文；Client 使用哈希派生本地槽，Server 明确 Unsupported。

`IGamePlatformSettingsMigration`

- `GetMigrationId()`
- `GetFromVersion()`
- `GetToVersion()`
- `Migrate()`

## 3. Descriptor / Value / Snapshot

`FGamePlatformSettingValue` 支持 Boolean、Integer、Number、String、Name，提供严格文本解析、类型校验、有限数检查和敏感诊断脱敏；String 统一限制4096字符。

`FGamePlatformSettingDescriptor` 描述 SettingId、Category、ValueType、默认值、范围、作用域、端侧、应用模式、平台限制、重启要求、敏感性和 SchemaVersion。

`bSensitive=true` 时 Descriptor 只能使用 Session 作用域；该标记不提供加密，凭据／Token／密钥不得注册为 Settings。

`FGamePlatformSettingsSnapshot`：

- Values：最终值 TMap。
- Revision：快照修订。
- SchemaVersion：当前设置档案版本。
- bDirty：User／待解析变化。
- bSaveInFlight：异步保存状态。
- LastResult：最近系统级结果。

## 4. 客户端设备设置 API

`IGamePlatformDeviceSettingsService::Get(UGameInstance&)`

负责原生 UGameUserSettings 设备设置：

- `GetSnapshot()`
- `ReloadFromDisk()`
- `StageDeviceSettings()`
- `ApplyStagedSettings(Preview/Commit)`
- `ConfirmPreview()`
- `CancelPreview()`
- `DiscardStagedSettings()`
- `Subscribe()/Unsubscribe()`

严格失败码包括：

- SettingsResolutionInvalid
- SettingsWindowModeInvalid
- SettingsFrameRateInvalid
- SettingsQualityInvalid
- SettingsPreviewActive
- SettingsMutationEnvironmentUnsupported
- SettingsCallbackReentrancy

## 5. 工程策略

`UGamePlatformSettingsProjectSettings` 暴露到 Project Settings（项目设置），管理：

- SchemaVersion
- bStrictValidation
- MaxProviders
- MaxDescriptors
- MaxSubscriptions
- LocalProfileSlotName

这些字段只控制基础设施策略，不存放神兽联盟玩法配置。

## 6. 用户上下文与账号边界

- Settings 不读取账号、Token、Online Snapshot，也不依赖 `GamePlatformOnline`。
- 登录组合层在认证完成后把一个稳定、不透明、最长128字符的 `UserContextKey` 传给 `SwitchUserContext()`。
- Client Persistence 只把该键的 SHA-1 派生值用于 SaveGame 槽隔离，原始键不进入文件名、Snapshot、诊断或普通日志。
- Logout 传空键，Runtime Reload 到默认／项目／Provider／Session链，不读取任何 User Profile。
- 切账号前必须先 `Apply()`、`Save()` 并等待 `bSaveInFlight=false`；否则返回 `SettingsUserContextSwitchBlocked`，避免丢失旧账号未保存设置。



## 2026-09-30现行合同补充

进程注册的Client持久化对象只作工厂；每个GI通过CreateScopedProvider拥有独立用户上下文。旧Client自定义Provider未提供作用域克隆时返回SettingsScopedPersistenceRequired，不共享可变用户键。设备UGameUserSettings进程语义保留。ClientContext交错A/B真实保存回归源已补，未执行磁盘验收。
