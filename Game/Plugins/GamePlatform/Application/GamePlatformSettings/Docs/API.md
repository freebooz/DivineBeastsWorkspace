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
- `Save()`：异步保存 User 层。
- `Reload()`：重建 Provider、读取持久层、执行 Migration、重新解析。
- `GetSnapshot()` / `GetDiagnostics()`：只读状态。
- `Subscribe()` / `Unsubscribe()`：低频批量变化事件。

## 2. Provider API

`IGamePlatformSettingsProvider`

- `GetProviderId()`
- `GetSettingDescriptors()`

上层模块只注册自己的 Descriptor，不修改平台 Registry。

`IGamePlatformSettingsPersistenceProvider`

- `SupportsRuntime()`
- `Load()`
- `BeginSave()`

`IGamePlatformSettingsMigration`

- `GetMigrationId()`
- `GetFromVersion()`
- `GetToVersion()`
- `Migrate()`

## 3. Descriptor / Value / Snapshot

`FGamePlatformSettingValue` 支持 Boolean、Integer、Number、String、Name，提供严格文本解析、类型校验、有限数检查和敏感诊断脱敏。

`FGamePlatformSettingDescriptor` 描述 SettingId、Category、ValueType、默认值、范围、作用域、端侧、应用模式、平台限制、重启要求、敏感性和 SchemaVersion。

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

