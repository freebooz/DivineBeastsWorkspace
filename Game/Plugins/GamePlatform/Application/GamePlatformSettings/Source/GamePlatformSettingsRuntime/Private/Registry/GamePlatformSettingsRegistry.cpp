#include "Registry/GamePlatformSettingsRegistry.h"

#include "Features/IModularFeatures.h"
#include "HAL/PlatformProperties.h"
#include "Validation/GamePlatformSettingsValidation.h"

FGamePlatformResult FGamePlatformSettingsRegistry::Rebuild(
    const int32 MaxProviders,
    const int32 MaxDescriptors,
    const EGamePlatformSettingRuntimeScope CurrentRuntime)
{
    TArray<IGamePlatformSettingsProvider*> Providers =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<IGamePlatformSettingsProvider>(
                IGamePlatformSettingsProvider::GetModularFeatureName());

    if (Providers.Num() > MaxProviders)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsProviderCapacity"),
            TEXT("设置Provider数量超过项目安全上限。"));
    }

    Providers.Sort(
        [](const IGamePlatformSettingsProvider& A,
           const IGamePlatformSettingsProvider& B)
        {
            return A.GetProviderId().LexicalLess(B.GetProviderId());
        });

    TMap<FName, FGamePlatformSettingDescriptor> NewDescriptors;
    TMap<FName, FName> NewOwners;
    TSet<FName> ProviderIds;
    const FName CurrentPlatform(FPlatformProperties::IniPlatformName());

    for (IGamePlatformSettingsProvider* Provider : Providers)
    {
        if (!Provider || Provider->GetProviderId().IsNone())
        {
            return FGamePlatformResult::Failure(
                TEXT("SettingsProviderIdentityInvalid"),
                TEXT("设置Provider为空或ProviderId为None。"));
        }

        const FName ProviderId = Provider->GetProviderId();
        if (ProviderIds.Contains(ProviderId))
        {
            return FGamePlatformResult::Failure(
                TEXT("SettingsProviderDuplicate"),
                TEXT("存在重复ProviderId，注册表拒绝使用加载顺序决胜。"));
        }
        ProviderIds.Add(ProviderId);

        TArray<FGamePlatformSettingDescriptor> ProviderDescriptors;
        Provider->GetSettingDescriptors(ProviderDescriptors);

        for (const FGamePlatformSettingDescriptor& Descriptor : ProviderDescriptors)
        {
#if UE_BUILD_SHIPPING
            if (Descriptor.bDevelopmentOnly)
            {
                continue;
            }
#endif
            if (!FGamePlatformSettingsValidation::SupportsRuntime(
                    Descriptor, CurrentRuntime))
            {
                continue;
            }

            if (!Descriptor.PlatformAllowList.IsEmpty() &&
                !Descriptor.PlatformAllowList.Contains(CurrentPlatform))
            {
                continue;
            }

            const FGamePlatformResult Validation =
                FGamePlatformSettingsValidation::ValidateDescriptor(Descriptor);
            if (!Validation.IsSuccess())
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsDescriptorInvalid"),
                    FString::Printf(
                        TEXT("Provider %s 的设置 %s 非法：%s"),
                        *ProviderId.ToString(),
                        *Descriptor.SettingId.ToString(),
                        *Validation.Message));
            }

            if (NewDescriptors.Contains(Descriptor.SettingId))
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsSettingIdDuplicate"),
                    FString::Printf(
                        TEXT("SettingId %s 被多个Provider重复注册。"),
                        *Descriptor.SettingId.ToString()));
            }

            if (NewDescriptors.Num() >= MaxDescriptors)
            {
                return FGamePlatformResult::Failure(
                    TEXT("SettingsDescriptorCapacity"),
                    TEXT("设置Descriptor数量超过项目安全上限。"));
            }

            NewDescriptors.Add(Descriptor.SettingId, Descriptor);
            NewOwners.Add(Descriptor.SettingId, ProviderId);
        }
    }

    Descriptors = MoveTemp(NewDescriptors);
    OwnerBySettingId = MoveTemp(NewOwners);
    ProviderCount = ProviderIds.Num();
    return FGamePlatformResult::Success();
}
