# GamePlatformSave API 说明

版本：0.2.0｜2026-09-29

## 1. 获取服务

```cpp
UGameInstance* GameInstance = ...;
IGamePlatformSaveService* SaveService =
    GameInstance ? IGamePlatformSaveService::Get(*GameInstance) : nullptr;
```

Dedicated Server（专用服务器）、Commandlet（命令行工具）或没有可用子系统时可能返回 `nullptr`。

## 2. 注册 Provider

Provider 用于声明一个 Namespace 的当前 SchemaVersion 和迁移逻辑。

```cpp
class FExampleLocalSaveProvider final : public IGamePlatformSaveProvider
{
public:
    virtual FName GetSaveNamespace() const override
    {
        return TEXT("ExampleLocal");
    }

    virtual int32 GetCurrentSchemaVersion() const override
    {
        return 2;
    }

    virtual FGamePlatformResult MigratePayload(
        int32 StoredSchemaVersion,
        int32 TargetSchemaVersion,
        TArray<uint8>& InOutPayload) const override
    {
        // 这里只做确定性内存迁移；禁止网络和阻塞磁盘IO。
        if (StoredSchemaVersion == 1 && TargetSchemaVersion == 2)
        {
            // ...迁移Payload...
            return FGamePlatformResult::Success();
        }

        return FGamePlatformResult::Failure(
            TEXT("ExampleSaveMigrationUnsupported"),
            TEXT("没有可用的本地存档迁移路径。"));
    }
};
```

注册：

```cpp
TSharedRef<FExampleLocalSaveProvider> Provider =
    MakeShared<FExampleLocalSaveProvider>();

FGamePlatformResult Result =
    SaveService->RegisterProvider(Provider);
```

同一 Namespace 重复注册会 Fail Closed（失败关闭）。

## 3. 构造逻辑 Key

```cpp
FGamePlatformSaveKey Key;
Key.Namespace = TEXT("ExampleLocal");
Key.ProfileKey = TEXT("opaque-profile-key");
Key.SlotName = TEXT("main");
```

注意：

- `ProfileKey` 不是密码、Token 或在线凭据；
- 调用方应传稳定、脱敏且非秘密的键；
- Key 原文不会直接写入文件名，最终使用 SHA-1 稳定摘要；
- 本插件不枚举其他账号文件，也不自动读取 Online 当前主体。

## 4. 异步保存

```cpp
FGamePlatformSaveRecord Record;
Record.Key = Key;
Record.SchemaVersion = Provider->GetCurrentSchemaVersion();
Record.Payload = SerializedPayload;

FGamePlatformResult StartResult;
FGamePlatformSaveRequestHandle Handle =
    SaveService->SaveRecordAsync(
        Record,
        [](const FGamePlatformSaveOperationResult& Completed)
        {
            if (!Completed.Result.IsSuccess())
            {
                // 处理本地保存失败；不能因此修改服务器权威数据。
            }
        },
        StartResult);
```

`StartResult` 表示是否成功进入异步队列；最终文件结果必须看 Callback。

如果对应 Namespace 已注册 Provider，保存记录的 SchemaVersion 必须等于 Provider 当前版本。

## 5. 异步加载

```cpp
FGamePlatformResult StartResult;
SaveService->LoadRecordAsync(
    Key,
    [](const FGamePlatformSaveOperationResult& Completed)
    {
        if (!Completed.Result.IsSuccess())
        {
            return;
        }

        const FGamePlatformSaveRecord& Record = Completed.Record;
        const bool bRecovered = Completed.bRecoveredFromBackup;
        const bool bMigrated = Completed.bMigrated;
        // ...反序列化Record.Payload...
    },
    StartResult);
```

若主档损坏／缺失而备份有效，`bRecoveredFromBackup=true`。

若注册 Provider 且旧记录版本低于当前版本，迁移在 Game Thread 执行，成功后 `bMigrated=true`。

若文件版本高于当前 Provider 支持版本，加载失败，不执行猜测式降级。

## 6. 异步删除

```cpp
FGamePlatformResult StartResult;
SaveService->DeleteRecordAsync(
    Key,
    [](const FGamePlatformSaveOperationResult& Completed)
    {
        // 删除不存在的记录按幂等成功处理。
    },
    StartResult);
```

删除范围只包含该稳定 Key 对应的主档和 `.bak`。

## 7. 回调生命周期

- 回调始终在 Game Thread；
- GameInstance 销毁时仍登记的请求会收到 `Cancelled`；
- 已经进入操作系统的后台磁盘调用不会被危险地强杀；旧代次结果不会重新进入新 GameInstance；
- Callback 执行期间 Save 服务拒绝同步反入，避免状态表在回调栈中被重入修改；
- 调用方 Lambda 如捕获 UObject，应自行使用 `TWeakObjectPtr` 或等效生命周期保护。

## 8. Provider 生命周期

Load 启动时会取得 Provider 的共享快照。即使随后调用 `UnregisterProvider`，已经开始的 Load 仍使用自己持有的 Provider 完成本次迁移。

## 9. 诊断

```cpp
const FGamePlatformSaveDiagnostics Diagnostics =
    SaveService->GetDiagnostics();
```

诊断只包含：Provider 数、待处理请求数、启动／完成／失败／恢复／迁移／拒绝计数。

不会包含：ProfileKey 原文、Payload、密码、Token 或文件绝对路径。
