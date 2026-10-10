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

## 2026-10-09 异步读取接口兼容

`IGamePlatformSettingsPersistenceProvider::BeginLoad`新增virtual，全部依赖插件必须重新编译，既有稳定身份/序列化字段不迁移。默认实现兼容调用旧Load，但旧Load仅适用于有界内存或服务器部署读取；客户端真实用户档案Load返回SettingsAsyncLoadRequired，消费者迁入BeginLoad。客户端先用UE5.8原生DoesSaveGameExistAsync区分不存在/错误，再AsyncLoadGameFromSlot；不存在可发布合法空User层，损坏或未知IO错误不能伪装成空成功。

Reload/换绑返回Success仅表示受理；订阅Snapshot.bLoading=false并检查LastResult才确认终态。Runtime候选注册表、层和快照在完整校验/迁移/解析成功后原子提交；失败保留上次只读快照。账号换绑成功时先撤销上账号User层并发布默认/Session投影，再异步加载新账号，失败不恢复旧账号数据。读取在飞期间普通修改明确拒绝，换绑可失效旧读取消费者；非法/相同用户键不取消合法在飞读取。

拓扑重载、Provider卸载和销毁推进LoadGeneration，重复/过期完成丢弃。逻辑取消只停止消费者接纳，不承诺强杀平台IO或回滚磁盘。新增反射bLoading字段默认false，现有档案类和槽名保持。
