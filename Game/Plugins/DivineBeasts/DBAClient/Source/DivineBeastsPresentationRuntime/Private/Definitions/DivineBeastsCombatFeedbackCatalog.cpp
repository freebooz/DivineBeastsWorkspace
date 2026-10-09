#include "Definitions/DivineBeastsCombatFeedbackCatalog.h"

bool UDivineBeastsCombatFeedbackCatalog::TryResolve(
    FName HeroDefinitionId,
    FName AbilityDefinitionId,
    FDivineBeastsCombatFeedbackEntry& OutEntry) const
{
    if (HeroDefinitionId.IsNone() || AbilityDefinitionId.IsNone())
    {
        return false;
    }
    const FDivineBeastsCombatFeedbackEntry* Matched = nullptr;
    for (const FDivineBeastsCombatFeedbackEntry& Entry : Entries)
    {
        if (Entry.HeroDefinitionId == HeroDefinitionId &&
            Entry.AbilityDefinitionId == AbilityDefinitionId)
        {
            if (Matched)
            {
                // 两项完全同键不是可预测覆盖：必须在资产发布前修复。
                return false;
            }
            Matched = &Entry;
        }
    }
    if (!Matched)
    {
        return false;
    }
    OutEntry = *Matched;
    return true;
}

bool UDivineBeastsCombatFeedbackCatalog::ValidateMappings(
    TArray<FString>& OutErrors) const
{
    OutErrors.Reset();
    TSet<FString> SeenKeys;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        const FDivineBeastsCombatFeedbackEntry& Entry = Entries[Index];
        if (Entry.HeroDefinitionId.IsNone() || Entry.AbilityDefinitionId.IsNone())
        {
            OutErrors.Add(FString::Printf(TEXT("第%d项缺少英雄或技能定义ID。"), Index + 1));
            continue;
        }

        const FString Key = Entry.HeroDefinitionId.ToString() +
            TEXT("/") + Entry.AbilityDefinitionId.ToString();
        if (SeenKeys.Contains(Key))
        {
            OutErrors.Add(FString::Printf(TEXT("重复的英雄技能映射：%s。"), *Key));
        }
        else
        {
            SeenKeys.Add(Key);
        }
        if (Entry.Profile.IsNull())
        {
            OutErrors.Add(FString::Printf(TEXT("英雄技能%s未指定反馈Profile资产。"), *Key));
        }
    }
    return OutErrors.IsEmpty();
}
