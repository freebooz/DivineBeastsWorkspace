# GamePlatformSettings 扩展开发指南

## 1. 原则
上层只通过 `IGamePlatformSettingsProvider（设置提供者接口）` 注册自己的 Descriptor（描述）；GamePlatformSettings 不反向依赖 MobaCommon 或 DivineBeasts。

## 2. MobaCommon 示例
```cpp
class FMobaSettingsProvider final : public IGamePlatformSettingsProvider
{
public:
    virtual FName GetProviderId() const override { return FName(TEXT("MobaCommon.Settings")); }
    virtual void GetSettingDescriptors(TArray<FGamePlatformSettingDescriptor>& Out) const override
    {
        FGamePlatformSettingDescriptor D;
        D.SettingId = FName(TEXT("Moba.UI.ShowObjectiveHints"));
        D.Category = FName(TEXT("Moba.UI"));
        D.ValueType = EGamePlatformSettingValueType::Boolean;
        D.DefaultValue = FGamePlatformSettingValue::MakeBool(true);
        D.DefaultLayer = EGamePlatformSettingLayer::ProviderDefault;
        D.PersistenceScope = EGamePlatformSettingScope::User;
        D.RuntimeScope = EGamePlatformSettingRuntimeScope::Client;
        Out.Add(D);
    }
};
```

## 3. DivineBeasts 示例
```cpp
class FDivineBeastsSettingsProvider final : public IGamePlatformSettingsProvider
{
public:
    virtual FName GetProviderId() const override { return FName(TEXT("DivineBeasts.Settings")); }
    virtual void GetSettingDescriptors(TArray<FGamePlatformSettingDescriptor>& Out) const override
    {
        FGamePlatformSettingDescriptor D;
        D.SettingId = FName(TEXT("DivineBeasts.HUD.ShowDamageNumbers"));
        D.Category = FName(TEXT("DivineBeasts.HUD"));
        D.ValueType = EGamePlatformSettingValueType::Boolean;
        D.DefaultValue = FGamePlatformSettingValue::MakeBool(true);
        D.DefaultLayer = EGamePlatformSettingLayer::ProjectDefault;
        D.PersistenceScope = EGamePlatformSettingScope::User;
        D.RuntimeScope = EGamePlatformSettingRuntimeScope::Client;
        Out.Add(D);
    }
};
```

上层模块在 StartupModule 注册 Provider，在 ShutdownModule 精确注销。禁止向基础插件增加 `SetDivineBeastsXXX()`。

## 4. 领域边界
Input 重绑由 GamePlatformInput；UI 可访问性由 GamePlatformUI；Camera、SFX 由各自服务消费 ChangeSet；LiveOps 动态运营参数不进入稳定用户 Settings；Definition 描述“内容是什么”，Settings 描述“用户选择什么／系统如何覆盖”。
