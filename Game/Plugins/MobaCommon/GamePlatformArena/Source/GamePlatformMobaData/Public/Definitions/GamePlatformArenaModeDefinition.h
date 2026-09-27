#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GamePlatformArenaModeDefinition.generated.h"

/** FGamePlatformArenaModeSpec（运行时不可变竞技模式规格）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMMOBADATA_API FGamePlatformArenaModeSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ArenaModeId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TeamCount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TeamSize = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TotalPlayers = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MapId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MinPlayers = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SelectionPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SpawnPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RespawnPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ScorePolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName WinConditionPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TimeLimitSeconds = 900;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName OvertimePolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Version = 1;

    bool Validate(FString& OutError) const;
};

/** UGamePlatformArenaModeDefinition（竞技模式主数据资产定义）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMMOBADATA_API UGamePlatformArenaModeDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName ArenaModeId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") int32 TeamCount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") int32 TeamSize = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") int32 TotalPlayers = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName MapId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") int32 MinPlayers = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName SelectionPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName SpawnPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName RespawnPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName ScorePolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName WinConditionPolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") int32 TimeLimitSeconds = 900;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") FName OvertimePolicyId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena") int32 Version = 1;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    UFUNCTION(BlueprintPure, Category="Arena") FGamePlatformArenaModeSpec ToSpec() const;
    bool ValidateDefinition(FString& OutError) const;
};
