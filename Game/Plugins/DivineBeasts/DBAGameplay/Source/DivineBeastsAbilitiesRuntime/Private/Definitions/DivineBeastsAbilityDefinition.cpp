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
