#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationContext.h"
#include "DivineBeastsPresentationContext.generated.h"

/**
 * FDivineBeastsPresentationProjectContext（神兽联盟项目表现上下文）。
 * 只保存稳定逻辑身份与表现提示，不保存UObject、AssetPath、Token、Ticket或Backend DTO。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationProjectContext
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WorldGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AvatarGeneration = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationLocalPlayerRelation LocalPlayerRelation =
        EGamePlatformPresentationLocalPlayerRelation::Unknown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationQualityTier QualityTier =
        EGamePlatformPresentationQualityTier::Unknown;

    bool IsValid(FString& OutError) const;
    FGamePlatformPresentationContext ToPlatformContext() const;
};
