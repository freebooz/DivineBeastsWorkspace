#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsProjectContext.generated.h"

/** EDivineBeastsProjectContextError（神兽联盟项目上下文验证错误）。 */
UENUM(BlueprintType)
enum class EDivineBeastsProjectContextError : uint8
{
    None,
    InvalidGameId,
    InvalidProjectId,
    InvalidServerRole,
    InvalidExperience,
    RoleExperienceMismatch,
    InvalidArenaMode,
    ArenaModeRequiresMainArena,
    MainArenaExperienceMismatch,
    InvalidContractVersion,
    MissingEnvironmentId,
    MissingBuildVersion
};

/**
 * FDivineBeastsProjectContext（神兽联盟项目上下文）。
 * 只保存项目级轻量稳定身份；不包含网络凭据、业务DTO或玩法对象。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSRUNTIME_API FDivineBeastsProjectContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName GameId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName ProjectId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName ServerRoleId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName ExperienceId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName ArenaModeId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName WorldId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName MapId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FString MatchId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FName EnvironmentId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FString BuildVersion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts")
    FString ContractVersion;

    void InitializeCanonicalIdentity();

    EDivineBeastsProjectContextError Validate() const;

    bool IsValid() const
    {
        return Validate() == EDivineBeastsProjectContextError::None;
    }
};
