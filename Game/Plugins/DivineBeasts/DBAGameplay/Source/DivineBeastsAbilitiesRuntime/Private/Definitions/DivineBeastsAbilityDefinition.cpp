#include "Definitions/DivineBeastsAbilityDefinition.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Engine/DataTable.h"

FGamePlatformResult UDivineBeastsAbilityDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Parent = Super::ValidateDefinition();
    if (!Parent.IsSuccess())
    {
        return Parent;
    }
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId))
    {
        return FGamePlatformResult::Failure(TEXT("AbilityHeroInvalid"),
            TEXT("技能定义的 HeroDefinitionId 不属于十二生肖有效英雄。"));
    }
    if (BalanceTable.IsNull() || LevelRowNames.IsEmpty() || LevelRowNames.Num() > 100)
    {
        return FGamePlatformResult::Failure(TEXT("AbilityBalanceMissing"),
            TEXT("技能必须配置合法数值表及1～100级真实行引用。"));
    }
    TSet<FName> Unique;
    for (FName Row : LevelRowNames)
    {
        if (Row.IsNone() || Unique.Contains(Row))
        {
            return FGamePlatformResult::Failure(TEXT("AbilityRowDuplicate"),
                TEXT("技能等级数值行不能为空或重复。"));
        }
        Unique.Add(Row);
    }

    if (bEightSlotDraftOnly)
    {
        // 原12套占位资源bEightSlotDraftOnly=false，绝不因设计字段增加而改变旧技能准入。
        static const TSet<FName> AllowedSlots =
        {
            TEXT("Primary"), TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3"),
            TEXT("Slot4"), TEXT("Slot5"), TEXT("Slot6"), TEXT("Slot7")
        };
        const FString ExpectedTag = FString(TEXT("Platform.Ability.Input.DivineBeasts.")) +
            DraftSlotId.ToString();
        if (!AllowedSlots.Contains(DraftSlotId) || !DraftInputTag.IsValid() ||
            DraftInputTag.ToString() != ExpectedTag ||
            DraftDisplayNameZh.IsEmptyOrWhitespace() || DraftDescriptionZh.IsEmptyOrWhitespace() ||
            DraftRoleId.IsNone() || DraftTargetModeId.IsNone() ||
            DraftHeroVFXProfileId.IsNone() || DraftPresentationSemanticId.IsNone() ||
            DraftActions.IsEmpty() || DraftActions.Num() > 8)
        {
            return FGamePlatformResult::Failure(
                TEXT("EightSlotDraftIncomplete"),
                TEXT("八槽技能设计资产缺少有效输入、身份、中文说明或结构化行为。"));
        }
        static const TSet<FName> AllowedActions =
        {
            TEXT("damage"), TEXT("movement"), TEXT("shield"),
            TEXT("healing"), TEXT("control"), TEXT("buff")
        };
        for (const FDivineBeastsEightSlotDraftAction& Action : DraftActions)
        {
            if (!AllowedActions.Contains(Action.ActionKindId) ||
                !FMath::IsFinite(Action.Magnitude) || Action.Magnitude < 0.0f ||
                !FMath::IsFinite(Action.DurationSeconds) ||
                Action.DurationSeconds < 0.0f || Action.DurationSeconds > 120.0f ||
                Action.HitCount < 1 || Action.HitCount > 8)
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftActionInvalid"),
                    TEXT("八槽技能设计行为缺少有效类型或包含非法幅值、持续秒数及命中段数。"));
            }
        }
    }
    return FGamePlatformResult::Success();
}

bool UDivineBeastsAbilityDefinition::TryGetLoadedBalance(
    int32 Level, FDivineBeastsAbilityBalanceRow& OutRow, FString& OutError) const
{
    const int32 Index = Level - 1;
    if (Level < 1 || !LevelRowNames.IsValidIndex(Index))
    {
        OutError = TEXT("技能等级超出该技能的已批准级数。");
        return false;
    }
    const UDataTable* Table = BalanceTable.Get();
    if (!Table || Table->GetRowStruct() != FDivineBeastsAbilityBalanceRow::StaticStruct())
    {
        OutError = TEXT("技能数值表尚未预加载，或行结构不匹配。");
        return false;
    }
    OutError.Reset();
    const FDivineBeastsAbilityBalanceRow* Row =
        Table->FindRow<FDivineBeastsAbilityBalanceRow>(LevelRowNames[Index], TEXT("AbilityBalance"), false);
    if (!Row || !Row->Validate(OutError) ||
        Row->Level != Level || Row->AbilityId != FName(*LogicalId.ToString()))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("技能数值行不存在，或技能编号/等级与定义不一致。");
        }
        return false;
    }
    OutRow = *Row;
    OutError.Reset();
    return true;
}
