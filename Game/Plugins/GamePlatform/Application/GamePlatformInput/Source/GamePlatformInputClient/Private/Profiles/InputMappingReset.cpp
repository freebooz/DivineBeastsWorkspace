// 平台客户端映射行所有权执行入口；被输入服务和瞬态UE测试消费，不做磁盘IO。
#include "Profiles/InputMappingReset.h"
#include "UserSettings/EnhancedInputUserSettings.h"
FGamePlatformResult ResetGamePlatformProfileMappings(UEnhancedInputUserSettings& Settings, const TSet<FName>& ProfileRows, FName RowName)
{
    check(IsInGameThread());
    const auto* KeyProfile = Settings.GetActiveKeyProfile();
    if (!KeyProfile) { return FGamePlatformResult::Failure(TEXT("InputUserSettingsUnavailable"), TEXT("当前键位Profile不可用。")); }
    TArray<FName> Rows;
    if (RowName.IsNone())
    {
        // 原生Profile是共享登记容器，不能按其全量行枚举扩大本配置的所有权。
        for (const FName Row : ProfileRows)
        { if (KeyProfile->GetPlayerMappingRows().Contains(Row)) { Rows.Add(Row); } }
    }
    else
    {
        if (!ProfileRows.Contains(RowName) || !KeyProfile->GetPlayerMappingRows().Contains(RowName))
        { return FGamePlatformResult::Failure(TEXT("RebindRowMissing"), TEXT("映射行未登记或不属于当前输入配置。")); }
        Rows.Add(RowName);
    }
    for (const FName Row : Rows)
    {
        FMapPlayerKeyArgs Args; Args.MappingName = Row; Args.Slot = EPlayerMappableKeySlot::Unspecified;
        FGameplayTagContainer FailureReason; Settings.ResetAllPlayerKeysInRow(Args, FailureReason);
        if (!FailureReason.IsEmpty())
        { return FGamePlatformResult::Failure(TEXT("ResetMappingRejected"), TEXT("原生输入拒绝重置；已发生的内存修改仍由调用者读取，不宣称事务回滚。")); }
    }
    return FGamePlatformResult::Success();
}
