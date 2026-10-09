#include "Adapters/MobaPresentationFactAdapters.h"

#include "GameFramework/Actor.h"
#include "Misc/Crc.h"
#include "Tags/MobaPresentationTags.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/GamePlatformCombatTypes.h"

namespace
{
    FString ActorPresentationId(const AActor* Actor)
    {
        return IsValid(Actor) ? Actor->GetFName().ToString() : FString();
    }

    FMobaPresentationAdaptedFact MakeBase(
        const FMobaPresentationFactIdentity& Identity,
        const FGameplayTag& Semantic)
    {
        FMobaPresentationAdaptedFact Fact;
        Fact.Identity = Identity;
        Fact.Semantic = Semantic;
        Fact.bTransient = true;
        Fact.Lifetime = EGamePlatformPresentationLifetime::Instant;
        return Fact;
    }
}

FGuid FMobaPresentationFactAdapters::MakeRevisionFactId(
    const FString& Scope,
    int32 Revision,
    uint32 Salt)
{
    const uint32 A = FCrc::StrCrc32(*Scope);
    const uint32 B = GetTypeHash(Revision);
    const uint32 C = HashCombine(A, B);
    const uint32 D = HashCombine(Salt, 0x4D4F4241u); // "MOBA"
    return FGuid(A, B, C, D);
}

void FMobaPresentationFactAdapters::FromCombatEvent(
    const FGamePlatformCombatEvent& Event,
    TArray<FMobaPresentationAdaptedFact>& OutFacts)
{
    FMobaPresentationFactIdentity Identity;
    Identity.FactId = Event.EventId;
    Identity.WorldGeneration = Event.WorldContextGeneration;
    Identity.AvatarGeneration = Event.TargetAvatarGeneration;
    Identity.bConfirmed = true;

    const FString SourceId = ActorPresentationId(Event.SourceActor);
    const FString TargetId = ActorPresentationId(Event.TargetActor);

    auto FillCombatContext = [&](FMobaPresentationAdaptedFact& Fact, float Magnitude)
    {
        Fact.Context.SourceEntityId = SourceId;
        Fact.Context.TargetEntityId = TargetId;        Fact.Context.AbilityId = Event.SourceAbilityId.ToString();
        Fact.Context.SourceLocation = IsValid(Event.SourceActor)
            ? Event.SourceActor->GetActorLocation()
            : FVector::ZeroVector;
        Fact.Context.TargetLocation = IsValid(Event.TargetActor)
            ? Event.TargetActor->GetActorLocation()
            : FVector::ZeroVector;
        Fact.Context.ImpactLocation = Event.ImpactPoint;
        Fact.Context.ImpactNormal = Event.ImpactNormal;
        Fact.Context.Magnitude = Magnitude;
        Fact.Context.WorldGeneration = Event.WorldContextGeneration;
        Fact.Context.AvatarGeneration = Event.TargetAvatarGeneration;
        Fact.Context.bConfirmed = true;
        Fact.Context.AdditionalTags = Event.ResultTags;
    };

    switch (Event.EventType)
    {
    case EGamePlatformCombatEventType::Damage:
    {
        FMobaPresentationAdaptedFact Hit =
            MakeBase(Identity, MobaPresentationTags::Combat_Hit);
        FillCombatContext(Hit, Event.AppliedMagnitude);
        OutFacts.Add(MoveTemp(Hit));

        if (Event.AppliedToShield > 0.0f)
        {
            FMobaPresentationAdaptedFact Shield =
                MakeBase(Identity, MobaPresentationTags::Combat_Shield_Hit);
            Shield.Identity.FactId = MakeRevisionFactId(
                Event.EventId.ToString(EGuidFormats::DigitsWithHyphens),
                Event.TargetAvatarGeneration,
                0x53484945u);
            FillCombatContext(Shield, Event.AppliedToShield);
            OutFacts.Add(MoveTemp(Shield));
        }
        break;
    }
    case EGamePlatformCombatEventType::Healing:
    {
        FMobaPresentationAdaptedFact Heal =
            MakeBase(Identity, MobaPresentationTags::Combat_Heal);
        FillCombatContext(Heal, Event.AppliedMagnitude);
        OutFacts.Add(MoveTemp(Heal));
        break;
    }
    case EGamePlatformCombatEventType::ControlApplied:
    {
        FMobaPresentationAdaptedFact Control =
            MakeBase(Identity, MobaPresentationTags::Combat_Control_Apply);
        FillCombatContext(Control, Event.AppliedMagnitude);
        OutFacts.Add(MoveTemp(Control));
        break;
    }
    case EGamePlatformCombatEventType::Death:
    {
        FMobaPresentationAdaptedFact Death =
            MakeBase(Identity, MobaPresentationTags::Character_Death);
        // 同次致死伤害可能先产生Death再产生Damage，两类事实不能共用去重GUID。
        Death.Identity.FactId = MakeRevisionFactId(
            Event.EventId.ToString(EGuidFormats::DigitsWithHyphens),
            Event.TargetAvatarGeneration, 0x44454144u);
        FillCombatContext(Death, 0.0f);
        Death.Priority = EGamePlatformPresentationPriority::High;
        OutFacts.Add(MoveTemp(Death));
        break;
    }
    case EGamePlatformCombatEventType::RespawnReset:
    {
        FMobaPresentationAdaptedFact Respawn =
            MakeBase(Identity, MobaPresentationTags::Character_Respawn);
        FillCombatContext(Respawn, 0.0f);
        Respawn.Context.AvatarGeneration = Event.TargetAvatarGeneration;
        OutFacts.Add(MoveTemp(Respawn));
        break;
    }
    case EGamePlatformCombatEventType::ControlRemoved:
    default:
        break;
    }
}

FMobaPresentationAdaptedFact FMobaPresentationFactAdapters::FromAbilityFact(
    const FMobaPresentationAbilityFact& Fact)
{
    FGameplayTag Semantic;
    switch (Fact.Type)
    {
    case EMobaPresentationAbilityFactType::CastStart:
        Semantic = MobaPresentationTags::Ability_Cast_Start;
        break;
    case EMobaPresentationAbilityFactType::CastRelease:
        Semantic = MobaPresentationTags::Ability_Cast_Release;
        break;
    case EMobaPresentationAbilityFactType::ProjectileSpawn:
        Semantic = MobaPresentationTags::Ability_Projectile_Spawn;
        break;
    case EMobaPresentationAbilityFactType::AreaWarning:
        Semantic = MobaPresentationTags::Ability_Area_Warning;
        break;
    }

    FMobaPresentationAdaptedFact Result = MakeBase(Fact.Identity, Semantic);
    Result.Context.AbilityId = Fact.AbilityId;
    Result.Context.SourceEntityId = Fact.SourceEntityId;
    Result.Context.TargetEntityId = Fact.TargetEntityId;
    Result.Context.SourceLocation = Fact.SourceLocation;
    Result.Context.TargetLocation = Fact.TargetLocation;
    Result.Context.ImpactLocation = Fact.TargetLocation;
    Result.Context.Magnitude = Fact.Magnitude;
    Result.Context.WorldGeneration = Fact.Identity.WorldGeneration;
    Result.Context.AvatarGeneration = Fact.Identity.AvatarGeneration;
    Result.Context.bPredicted = Fact.Identity.bPredicted;
    Result.Context.bConfirmed = Fact.Identity.bConfirmed;
    return Result;
}

FMobaPresentationAdaptedFact FMobaPresentationFactAdapters::FromStatusFact(
    const FMobaPresentationStatusFact& Fact)
{
    const FGameplayTag Semantic =
        Fact.Type == EMobaPresentationStatusFactType::Apply
        ? MobaPresentationTags::Status_Apply
        : MobaPresentationTags::Status_Remove;

    FMobaPresentationAdaptedFact Result = MakeBase(Fact.Identity, Semantic);
    Result.Context.StatusId = Fact.StatusId;
    Result.Context.TargetEntityId = Fact.TargetEntityId;
    Result.Context.WorldGeneration = Fact.Identity.WorldGeneration;
    Result.Context.AvatarGeneration = Fact.Identity.AvatarGeneration;
    Result.Context.bPredicted = Fact.Identity.bPredicted;
    Result.Context.bConfirmed = Fact.Identity.bConfirmed;

    if (Fact.Type == EMobaPresentationStatusFactType::Apply)
    {
        Result.bTransient = false;
        Result.Lifetime = EGamePlatformPresentationLifetime::Persistent;
        Result.Context.Magnitude = Fact.DisplayDurationSeconds;
    }
    return Result;
}

FMobaPresentationAdaptedFact FMobaPresentationFactAdapters::FromCharacterFact(
    const FMobaPresentationCharacterFact& Fact)
{
    const FGameplayTag Semantic =
        Fact.Type == EMobaPresentationCharacterFactType::Death
        ? MobaPresentationTags::Character_Death
        : MobaPresentationTags::Character_Respawn;

    FMobaPresentationAdaptedFact Result = MakeBase(Fact.Identity, Semantic);
    Result.Context.TargetCharacterId = Fact.CharacterId;
    Result.Context.TargetEntityId = Fact.CharacterId;
    Result.Context.SourceEntityId = Fact.RelatedEntityId;
    Result.Context.TargetLocation = Fact.Location;
    Result.Context.ImpactLocation = Fact.Location;
    Result.Context.WorldGeneration = Fact.Identity.WorldGeneration;
    Result.Context.AvatarGeneration = Fact.Identity.AvatarGeneration;
    Result.Priority = EGamePlatformPresentationPriority::High;
    return Result;
}

FMobaPresentationAdaptedFact FMobaPresentationFactAdapters::FromArenaFact(
    const FMobaPresentationArenaFact& Fact)
{
    FGameplayTag Semantic;
    switch (Fact.Type)
    {
    case EMobaPresentationArenaFactType::MatchStart:
        Semantic = MobaPresentationTags::Arena_Match_Start;
        break;
    case EMobaPresentationArenaFactType::MatchEnd:
        Semantic = MobaPresentationTags::Arena_Match_End;
        break;
    case EMobaPresentationArenaFactType::ScoreChanged:
        Semantic = MobaPresentationTags::Arena_Score_Changed;
        break;
    case EMobaPresentationArenaFactType::ObjectiveCompleted:
        Semantic = MobaPresentationTags::Arena_Objective_Completed;
        break;
    }

    FMobaPresentationAdaptedFact Result = MakeBase(Fact.Identity, Semantic);
    Result.Context.MatchId = Fact.MatchId;
    Result.Context.ArenaModeId = Fact.ArenaModeId;
    Result.Context.TeamId = Fact.TeamId;
    Result.Context.Magnitude = static_cast<float>(Fact.Value);
    Result.Context.WorldGeneration = Fact.Identity.WorldGeneration;
    if (Fact.Type == EMobaPresentationArenaFactType::ScoreChanged)
    {
        Result.bTransient = false;
        Result.Lifetime = EGamePlatformPresentationLifetime::Persistent;
    }
    return Result;
}
