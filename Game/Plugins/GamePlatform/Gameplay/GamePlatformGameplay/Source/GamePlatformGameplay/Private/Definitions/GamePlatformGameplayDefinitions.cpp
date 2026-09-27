#include "Definitions/GamePlatformExperienceDefinition.h"
#include "Definitions/GamePlatformPawnDefinition.h"
#include "Settings/GamePlatformGameplaySettings.h"
#include "Types/GamePlatformPlayerLifecycle.h"
#include "Types/GamePlatformSpawnRequest.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

namespace
{
FGamePlatformResult Invalid(FName Code)
{
    return FGamePlatformResult::Failure(Code, TEXT("玩法定义字段未满足声明规则，请按错误码定位；不会自动回退到默认资产。"));
}
bool ValidId(const FPrimaryAssetId& Id)
{
    FGamePlatformId Parsed;
    return Id.PrimaryAssetType == UGamePlatformPrimaryDataAsset::DefinitionAssetType()
        && FGamePlatformId::TryParse(Id.PrimaryAssetName.ToString(), Parsed);
}
bool ValidTimeout(float Seconds, float Maximum)
{
    return FMath::IsFinite(Seconds) && Seconds > 0.f && Seconds <= Maximum;
}
FGamePlatformResult ValidateShared(const UGamePlatformDefinitionBase& Definition, const TArray<FPrimaryAssetId>& Ids)
{
    TSet<FPrimaryAssetId> Seen;
    for (const FPrimaryAssetId& Id : Ids)
    {
        if (!ValidId(Id) || Id == Definition.GetPrimaryAssetId() || Seen.Contains(Id)
            || !Definition.RequiredDefinitions.Contains(Id)) return Invalid(TEXT("InvalidSharedDependency"));
        Seen.Add(Id);
    }
    return FGamePlatformResult::Success();
}
}

FGamePlatformResult UGamePlatformGameplaySettings::ValidateSettings() const
{
    if (MaximumWaitingPlayers < 1 || MaximumWaitingPlayers > 1024
        || !ValidTimeout(MaximumPhaseTimeoutSeconds, 600.f) || MaximumPhaseTimeoutSeconds < 1.f
        || !ValidTimeout(MinimumPreparationReportIntervalSeconds, 5.f) || MinimumPreparationReportIntervalSeconds < 0.01f
        || MaximumSpawnPolicies < 1 || MaximumSpawnPolicies > 128
        || MaximumSpawnCandidates < 1 || MaximumSpawnCandidates > 1024) return Invalid(TEXT("InvalidGameplaySettings"));
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformExperienceDefinition::ValidateDefinition() const
{
    check(IsInGameThread());
    const auto Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) return Base;
    const auto* Settings = GetDefault<UGamePlatformGameplaySettings>();
    const auto SettingsResult = Settings->ValidateSettings();
    if (!SettingsResult.IsSuccess()) return SettingsResult;
    if (Purpose.IsNone() || SpawnPolicyId.IsNone() || SupportedWorldIds.IsEmpty()) return Invalid(TEXT("MissingExperienceContext"));
    TSet<FGamePlatformId> Worlds;
    for (const auto& Id : SupportedWorldIds)
    {
        if (!Id.IsValid() || Worlds.Contains(Id)) return Invalid(TEXT("InvalidSupportedWorld"));
        Worlds.Add(Id);
    }
    if (!ValidId(DefaultPawnDefinitionId)) return Invalid(TEXT("MissingPawnDefinition"));
    if (!RequiredDefinitions.Contains(DefaultPawnDefinitionId)) return Invalid(TEXT("PawnDependencyMissing"));
    const auto Shared = ValidateShared(*this, SharedDefinitions);
    if (!Shared.IsSuccess()) return Shared;
    TSet<FPrimaryAssetId> Servers;
    for (const auto& Id : ServerDefinitions)
    {
        if (!ValidId(Id) || Id == GetPrimaryAssetId() || Servers.Contains(Id) || RequiredDefinitions.Contains(Id))
            return Invalid(TEXT("InvalidServerDependency"));
        Servers.Add(Id);
    }
    if (!ValidTimeout(PlayerExperienceTimeoutSeconds, Settings->MaximumPhaseTimeoutSeconds)
        || !ValidTimeout(SpawnTimeoutSeconds, Settings->MaximumPhaseTimeoutSeconds)
        || !ValidTimeout(ClientPreparationTimeoutSeconds, Settings->MaximumPhaseTimeoutSeconds)
        || MaximumWaitingPlayers < 1 || MaximumWaitingPlayers > Settings->MaximumWaitingPlayers)
        return Invalid(TEXT("InvalidGameplayLimits"));
    if (RespawnPolicy != EGamePlatformRespawnPolicy::Disabled && RespawnPolicy != EGamePlatformRespawnPolicy::ServerAuthorized)
        return Invalid(TEXT("InvalidRespawnPolicy"));
    if (AssemblyEntries.Num() > 64) return Invalid(TEXT("TooManyAssemblyEntries"));
    TSet<FName> Names;
    for (const auto& Entry : AssemblyEntries)
    {
        if (Entry.AssemblyId.IsNone() || Entry.FactoryId.IsNone()) return Invalid(TEXT("MissingAssemblyKey"));
        if (Names.Contains(Entry.AssemblyId)) return Invalid(TEXT("DuplicateAssemblyId"));
        Names.Add(Entry.AssemblyId);
    }
    for (const auto& Entry : AssemblyEntries)
    {
        TSet<FName> Dependencies;
        for (FName Id : Entry.Dependencies)
        {
            if (!Names.Contains(Id) || Dependencies.Contains(Id)) return Invalid(TEXT("InvalidAssemblyDependency"));
            Dependencies.Add(Id);
        }
    }
    // 有界拓扑消去；有环时一轮不会取得进展，不递归遍历任意用户图。
    TSet<FName> Ordered;
    while (Ordered.Num() < Names.Num())
    {
        const int32 Before = Ordered.Num();
        for (const auto& Entry : AssemblyEntries)
            if (!Ordered.Contains(Entry.AssemblyId) && !Entry.Dependencies.ContainsByPredicate([&](FName Id) { return !Ordered.Contains(Id); }))
                Ordered.Add(Entry.AssemblyId);
        if (Before == Ordered.Num()) return Invalid(TEXT("AssemblyDependencyCycle"));
    }
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformPawnDefinition::ValidateDefinition() const
{
    check(IsInGameThread());
    const auto Base = Super::ValidateDefinition();
    if (!Base.IsSuccess()) return Base;
    if (PawnClass.IsNull() || !FPackageName::IsValidObjectPath(PawnClass.ToSoftObjectPath().ToString()))
        return Invalid(TEXT("MissingPawnClass"));
    const FVector Extent = SpawnEnvelopeHalfExtentCentimeters;
    if (Extent.ContainsNaN() || Extent.GetMin() <= 0.0 || Extent.GetMax() > 10000.0 || PawnPurpose.IsNone())
        return Invalid(TEXT("InvalidSpawnEnvelope"));
    return ValidateShared(*this, SharedDefinitions);
}

bool FGamePlatformVerifiedPlayerContext::IsStructurallyValid() const
{
    const bool bNoPawnOverride = !PawnDefinitionId.IsValid() && PawnDefinitionId.PrimaryAssetName.IsNone()
        && PawnDefinitionId.PrimaryAssetType.IsNone();
    return AdmissionId.IsValid() && ParticipantId.IsValid() && ExperienceId.IsValid()
        && !AssignmentId.IsEmpty() && AssignmentId.Len() <= 128
        && !ServerInstanceId.IsEmpty() && ServerInstanceId.Len() <= 128
        && ServerStartGeneration > 0 && ConnectionGeneration > 0 && SessionEpoch > 0
        && (bNoPawnOverride || ValidId(PawnDefinitionId));
}

bool FGamePlatformSpawnCandidate::IsStructurallyValid(const UWorld& ExpectedWorld) const
{
    const bool bEmptyRegion = RegionId.Namespace.IsEmpty() && RegionId.Name.IsEmpty() && RegionId.LogicalVersion == 1;
    return !CandidateId.IsNone() && Source.IsValid() && Source->GetWorld() == &ExpectedWorld
        && Transform.IsValid() && Transform.GetScale3D().Equals(FVector::OneVector)
        && (bEmptyRegion || RegionId.IsValid());
}
