#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformPresentationContext.h"
#include "GamePlatformPresentationCatalog.generated.h"

/** EGamePlatformPresentationCatalogScope（表现目录作用域，数值越大优先级越高）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationCatalogScope : uint8
{
    Platform = 0,
    Moba = 1,
    Project = 2,
    ContentPack = 3
};

/** EGamePlatformPresentationCatalogResolveResult（目录解析结果）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationCatalogResolveResult : uint8
{
    Resolved,
    NoMatch,
    Ambiguous
};

/** FGamePlatformPresentationRegistrationHandle（可撤销注册句柄）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationRegistrationHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};

/** FGamePlatformPresentationContextQuery（目录上下文匹配查询）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationContextQuery
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProjectId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HeroDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SkinId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WorldId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ExperienceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RegionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArenaModeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ContentPackId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlatformId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationQualityTier QualityTier =
        EGamePlatformPresentationQualityTier::Unknown;

    bool Matches(const FGamePlatformPresentationContext& Context) const;
    int32 GetSpecificity() const;
};

/** FGamePlatformPresentationCatalogEntry（中立表现目录条目）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EntryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag SemanticTag;
    /** 仅显式允许时供子语义逐级回退；默认false保留精确匹配，不能靠高Scope遮盖精确条目。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowParentFallback = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGamePlatformPresentationContextQuery ContextQuery;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ProviderChannel = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationCatalogScope Scope =
        EGamePlatformPresentationCatalogScope::Platform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Specificity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ContentRevision;

    bool IsValid() const
    {
        return !EntryId.IsNone() &&
               SemanticTag.IsValid() &&
               !ProviderChannel.IsNone() &&
               !DefinitionId.IsNone() &&
               !ContentRevision.TrimStartAndEnd().IsEmpty();
    }
};

/** FGamePlatformPresentationCatalogFragment（可注册目录片段）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationCatalogFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName FragmentId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationCatalogScope Scope =
        EGamePlatformPresentationCatalogScope::Project;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerScopeId = NAME_None;
    /** 注册生命周期作用域：World Travel时自动清理World级片段。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationContextScope LifecycleScope =
        EGamePlatformPresentationContextScope::Session;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGamePlatformPresentationCatalogEntry> Entries;

    bool IsValid() const;
};

/** FGamePlatformPresentationResolvedEntry（目录解析结果条目）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationResolvedEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName EntryId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName ProviderChannel = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName DefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FString ContentRevision;
    UPROPERTY(BlueprintReadOnly) EGamePlatformPresentationCatalogScope Scope =
        EGamePlatformPresentationCatalogScope::Platform;
};
