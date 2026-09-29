# GamePlatformSettings 配置版本迁移

`UGamePlatformSettingsProjectSettings::SchemaVersion` 是用户设置档案当前版本。客户端 Profile 保存自身版本；服务器部署配置只读，不做运行时回写迁移。

`IGamePlatformSettingsMigration` 每个实现只允许 `FromVersion → FromVersion + 1`。Reload 时 Runtime 逐步寻找唯一迁移；缺失、重复或跳版本都会 Fail Closed。

迁移只操作内存 User 层副本，不直接写磁盘。完整迁移、校验和解析成功后才使用新快照；失败不会删除或覆盖原 Profile。

```cpp
class FSettingsMigrationV1ToV2 final : public IGamePlatformSettingsMigration
{
public:
    virtual FName GetMigrationId() const override { return FName(TEXT("GamePlatform.Settings.V1ToV2")); }
    virtual int32 GetFromVersion() const override { return 1; }
    virtual int32 GetToVersion() const override { return 2; }
    virtual FGamePlatformResult Migrate(TMap<FName, FGamePlatformSettingValue>& Values) const override
    {
        if (FGamePlatformSettingValue* Old = Values.Find(FName(TEXT("Platform.Legacy.HudScale"))))
        {
            Values.Add(FName(TEXT("Platform.UI.HudScale")), *Old);
            Values.Remove(FName(TEXT("Platform.Legacy.HudScale")));
        }
        return FGamePlatformResult::Success();
    }
};
```

提升 SchemaVersion 前必须补齐旧版本、跨多版本和失败恢复测试，并验证旧文件在迁移失败后保持不变。
