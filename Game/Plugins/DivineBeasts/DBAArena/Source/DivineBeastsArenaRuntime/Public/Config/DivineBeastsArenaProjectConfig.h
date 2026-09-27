#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformArenaModeDefinition.h"
#include "DivineBeastsArenaProjectConfig.generated.h"

/** EDivineBeastsArenaConfigState（项目竞技配置状态）。 */
UENUM(BlueprintType)
enum class EDivineBeastsArenaConfigState : uint8
{
    NotConfigured,
    DevelopmentOnly,
    ProductionReady
};

/** EDivineBeastsArenaFeatureState（项目竞技可选功能状态）。 */
UENUM(BlueprintType)
enum class EDivineBeastsArenaFeatureState : uint8
{
    NotConfigured,
    Unsupported,
    Supported
};

/** EDivineBeastsArenaDuplicateHeroPolicy（重复英雄策略）。 */
UENUM(BlueprintType)
enum class EDivineBeastsArenaDuplicateHeroPolicy : uint8
{
    Unspecified,
    Allowed,
    DisallowWithinTeam,
    DisallowGlobal
};

/**
 * FDivineBeastsArenaProjectModeSpec（神兽联盟项目竞技模式规格）。
 * 固定结构字段与产品待确认字段分离；未批准值保持NotConfigured/None/0。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSARENARUNTIME_API FDivineBeastsArenaProjectModeSpec
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ArenaModeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TeamCount = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TeamSize = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TotalPlayers = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ServerRoleId = TEXT("GameServer.Role.MainArena");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ExperienceId = TEXT("Experience.MainArena.Main");

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MapId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SelectionPolicyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SpawnPolicyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RespawnPolicyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ScorePolicyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName WinConditionPolicyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName OvertimePolicyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TimeLimitSeconds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ProjectRuleRevision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString ContentRevision;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 HeroCatalogRevision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly) EDivineBeastsArenaConfigState ConfigState = EDivineBeastsArenaConfigState::NotConfigured;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EDivineBeastsArenaFeatureState PickBanState = EDivineBeastsArenaFeatureState::NotConfigured;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EDivineBeastsArenaDuplicateHeroPolicy DuplicateHeroPolicy = EDivineBeastsArenaDuplicateHeroPolicy::Unspecified;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EDivineBeastsArenaFeatureState OvertimeState = EDivineBeastsArenaFeatureState::NotConfigured;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EDivineBeastsArenaFeatureState SuddenDeathState = EDivineBeastsArenaFeatureState::NotConfigured;

    bool ValidateStructure(FString& OutError) const;
    bool ValidateProduction(FString& OutError) const;
    bool TryBuildPlatformSpec(FGamePlatformArenaModeSpec& OutSpec, FString& OutError) const;
};

/**
 * UDivineBeastsArenaModeDefinition（神兽联盟项目竞技模式Definition）。
 * 非空扩展：补充项目Rule/Content/HeroCatalog修订及Pick/Ban/DuplicateHero显式状态。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSARENARUNTIME_API UDivineBeastsArenaModeDefinition
    : public UGamePlatformArenaModeDefinition
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    FName ServerRoleId = TEXT("GameServer.Role.MainArena");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    FName ExperienceId = TEXT("Experience.MainArena.Main");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    int32 ProjectRuleRevision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    FString ContentRevision;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    int32 HeroCatalogRevision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    EDivineBeastsArenaConfigState ConfigState = EDivineBeastsArenaConfigState::NotConfigured;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    EDivineBeastsArenaFeatureState PickBanState = EDivineBeastsArenaFeatureState::NotConfigured;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    EDivineBeastsArenaDuplicateHeroPolicy DuplicateHeroPolicy = EDivineBeastsArenaDuplicateHeroPolicy::Unspecified;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    EDivineBeastsArenaFeatureState OvertimeState = EDivineBeastsArenaFeatureState::NotConfigured;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Arena")
    EDivineBeastsArenaFeatureState SuddenDeathState = EDivineBeastsArenaFeatureState::NotConfigured;

    FDivineBeastsArenaProjectModeSpec ToProjectSpec() const;
    bool ValidateProjectDefinition(bool bRequireProduction, FString& OutError) const;
};
