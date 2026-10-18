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
#if WITH_EDITORONLY_DATA
        // 提示词只在编辑器检查与保存；运行时不因Cook剥离该审核字段而误拒绝定义。
        if (DraftDesignPromptZh.TrimStartAndEnd().IsEmpty())
        {
            return FGamePlatformResult::Failure(
                TEXT("EightSlotDraftDesignPromptMissing"),
                TEXT("八槽技能草案缺少供人工审核的中文设计提示词。"));
        }
#endif
        // 原占位资源默认仍为false；草案目录身份不能改变旧技能准入。
        static const TSet<FName> AllowedSlots =
        {
            TEXT("Primary"), TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3"),
            TEXT("Slot4"), TEXT("Slot5"), TEXT("Slot6"), TEXT("Slot7")
        };
        const FString ExpectedTag = FString(TEXT("Platform.Ability.Input.DivineBeasts.")) +
            DraftSlotId.ToString();
        if (!AllowedSlots.Contains(DraftSlotId) || !DraftInputTag.IsValid() ||
            DraftInputTag.ToString() != ExpectedTag ||
            DraftInputActionId.IsNone() || DraftDefaultKeyId.IsNone() ||
            DraftDisplayOrder < 1 || DraftDisplayOrder > 8 ||
            DraftDisplayNameZh.IsEmptyOrWhitespace() || DraftDescriptionZh.IsEmptyOrWhitespace() ||
            DraftRoleId.IsNone() || DraftTargetModeId.IsNone() ||
            DraftAssetOwnerId != FName(TEXT("DBAGameplay")) ||
            DraftCatalogId != FName(TEXT("SSLM.ZodiacEightSlotSkills.V2")) ||
            DraftSourceSha256.Len() != 64 ||
            DraftHeroVFXProfileId.IsNone() || DraftVFXCatalogKey.IsNone() ||
            DraftIconPlaceholderId != FName(TEXT("SSLM.Placeholder.Icon.Unbound")) ||
            DraftVFXPlaceholderId != FName(TEXT("SSLM.Placeholder.VFX.Unbound")) ||
            !bDraftPresentationUnbound ||
            DraftPresentationSemanticId.IsNone() || DraftPresentationSemanticIds.IsEmpty() ||
            DraftPresentationSemanticIds[0] != DraftPresentationSemanticId ||
            DraftActions.IsEmpty() || DraftActions.Num() > 8)
        {
            return FGamePlatformResult::Failure(
                TEXT("EightSlotDraftIncomplete"),
                TEXT("八槽技能缺少资产归属、身份摘要、输入槽、中文说明或结构化行为。"));
        }

        for (const TCHAR Character : DraftSourceSha256)
        {
            const bool bHexDigit = (Character >= '0' && Character <= '9') ||
                (Character >= 'a' && Character <= 'f');
            if (!bHexDigit)
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftSourceHashInvalid"),
                    TEXT("八槽技能来源摘要必须是小写64位SHA-256。"));
            }
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
                Action.HitCount < 1 || Action.HitCount > 8 ||
                Action.SourcePayloadJson.IsEmpty() ||
                !Action.SourcePayloadJson.StartsWith(TEXT("{")) ||
                !Action.SourcePayloadJson.EndsWith(TEXT("}")))
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftActionInvalid"),
                    TEXT("八槽行为包含未知类型、非法数值或缺少原始策划JSON。"));
            }

            const bool bNeedsModifier =
                Action.ActionKindId == TEXT("control") ||
                Action.ActionKindId == TEXT("buff");
            const bool bNeedsDamageType = Action.ActionKindId == TEXT("damage");
            if (bNeedsModifier != !Action.ModifierId.IsNone() ||
                bNeedsDamageType != !Action.DamageTypeId.IsNone() ||
                (Action.bServerNavValidation !=
                    (Action.ActionKindId == TEXT("movement"))) ||
                (Action.bAllowRevive &&
                    Action.ActionKindId != TEXT("healing")))
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftActionSemanticsInvalid"),
                    TEXT("八槽行为的修饰身份、伤害类型或服务器校验语义不完整。"));
            }
            if (Action.bAllowRevive)
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftReviveNotApproved"),
                    TEXT("当前八槽技能草案没有审批复活行为。"));
            }

            FName ExpectedMagnitudeSemantic = NAME_None;
            if (Action.ActionKindId == TEXT("damage"))
            {
                ExpectedMagnitudeSemantic = TEXT("damage_points");
            }
            else if (Action.ActionKindId == TEXT("movement"))
            {
                ExpectedMagnitudeSemantic = TEXT("distance_cm");
            }
            else if (Action.ActionKindId == TEXT("shield") ||
                     Action.ActionKindId == TEXT("healing"))
            {
                ExpectedMagnitudeSemantic = TEXT("max_health_fraction");
            }
            else if (Action.ActionKindId == TEXT("control"))
            {
                ExpectedMagnitudeSemantic = TEXT("slow_fraction");
            }
            else if (Action.ActionKindId == TEXT("buff"))
            {
                ExpectedMagnitudeSemantic = TEXT("magnitude_fraction");
            }
            if (Action.MagnitudeSemanticId != ExpectedMagnitudeSemantic)
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftMagnitudeSemanticInvalid"),
                    TEXT("八槽行为幅值单位与行为类型不匹配。"));
            }
            if (bNeedsDamageType &&
                Action.DamageTypeId != TEXT("Physical") &&
                Action.DamageTypeId != TEXT("Magic") &&
                Action.DamageTypeId != TEXT("Untyped"))
            {
                return FGamePlatformResult::Failure(
                    TEXT("EightSlotDraftDamageTypeInvalid"),
                    TEXT("八槽伤害行为类型未登记。"));
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

