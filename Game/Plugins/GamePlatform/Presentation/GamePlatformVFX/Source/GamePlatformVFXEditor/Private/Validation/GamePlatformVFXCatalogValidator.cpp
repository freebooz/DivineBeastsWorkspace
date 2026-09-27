#include "Validation/GamePlatformVFXCatalogValidator.h"
#include "Catalogs/GamePlatformVFXCatalog.h"

void FGamePlatformVFXCatalogValidator::Validate(
    const UGamePlatformVFXCatalog& Catalog,
    TArray<FGamePlatformVFXValidationIssue>& OutIssues)
{
    TSet<FString> UniqueKeys;

    for (const FGamePlatformVFXCatalogEntry& Entry : Catalog.Entries)
    {
        if (Entry.Definition.IsNull())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Catalog.MissingDefinition");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CatalogMissingDefinition", "Catalog 条目必须指定 Definition。");
        }

        if (Entry.DefinitionId.IsNone())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Catalog.MissingDefinitionId");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CatalogMissingDefinitionId", "Catalog条目必须声明逻辑DefinitionId，不能让上层依赖资产路径。");
        }

        if (Entry.RequiredContextTags.HasAny(Entry.BlockedContextTags))
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Catalog.ConflictingTags");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CatalogConflictingTags", "Catalog RequiredContextTags与BlockedContextTags不能重叠。");
        }

        const FString Key = FString::Printf(
            TEXT("%s|%s|%s|%s|%d|%d|%d|%d"),
            *Entry.SemanticTag.ToString(),
            *Entry.ContextId.ToString(),
            *Entry.RequiredContextTags.ToStringSimple(false),
            *Entry.PlatformId.ToString(),
            static_cast<int32>(Entry.Scope),
            Entry.Specificity,
            Entry.Priority,
            Entry.bAnyQuality ? -1 : static_cast<int32>(Entry.QualityTier));

        if (UniqueKeys.Contains(Key))
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Catalog.AmbiguousEntry");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "CatalogAmbiguousEntry", "Catalog存在同语义、同上下文、同specificity/scope/priority/quality的歧义条目：{0}"),
                FText::FromString(Key));
        }

        UniqueKeys.Add(Key);
    }
}
