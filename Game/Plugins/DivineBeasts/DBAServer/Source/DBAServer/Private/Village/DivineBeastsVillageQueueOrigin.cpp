#include "Village/DivineBeastsVillageQueueOrigin.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace DivineBeasts::VillageQueueOrigin
{
bool Capture(const ACharacter& Pawn, const FString& CharacterId, FGamePlatformVerifiedReturnSpawn& OutSpawn)
{
    const auto* Movement = Pawn.GetCharacterMovement();
    const UWorld* World = Pawn.GetWorld();
    const FVector Location = Pawn.GetActorLocation();
    const double Yaw = Pawn.GetActorRotation().GetNormalized().Yaw;
    if (!Pawn.HasAuthority() || !World || World->bIsTearingDown || !Movement || !Movement->IsMovingOnGround() ||
        !Movement->CurrentFloor.IsWalkableFloor() || Cast<APawn>(Movement->CurrentFloor.HitResult.GetActor()) ||
        CharacterId.IsEmpty() || Location.ContainsNaN() || !FMath::IsFinite(Yaw))
    {
        return false;
    }
    OutSpawn = {CharacterId, World->GetOutermost()->GetName(), Location, Yaw};
    return true;
}

FHttpRequestPtr MakeReport(const FGamePlatformServerVerifiedAdmission& Admission, const FString& QueueTicketId,
    const FGamePlatformVerifiedReturnSpawn& Spawn)
{
    FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_BASE_URL"));
    BaseUrl.RemoveFromEnd(TEXT("/"));
    const FString Token = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_INTERNAL_TOKEN"));
    const bool bHttps = BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#if UE_BUILD_SHIPPING
    const bool bAllowedScheme = bHttps;
#else
    const bool bAllowedScheme = bHttps || BaseUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase);
#endif
    if (!bAllowedScheme || Token.IsEmpty() || BaseUrl.Len() > 2048 || Token.Len() > 4096 ||
        BaseUrl.Contains(TEXT("?")) || BaseUrl.Contains(TEXT("#")) || BaseUrl.Contains(TEXT("\r")) || BaseUrl.Contains(TEXT("\n")) ||
        Token.Contains(TEXT("\r")) || Token.Contains(TEXT("\n")))
    {
        return nullptr;
    }

    const auto Location = MakeShared<FJsonObject>();
    Location->SetStringField(TEXT("characterId"), Spawn.CharacterId);
    Location->SetStringField(TEXT("mapPackage"), Spawn.MapPackage);
    Location->SetNumberField(TEXT("xCm"), Spawn.LocationCm.X);
    Location->SetNumberField(TEXT("yCm"), Spawn.LocationCm.Y);
    Location->SetNumberField(TEXT("zCm"), Spawn.LocationCm.Z);
    Location->SetNumberField(TEXT("yawDegrees"), Spawn.YawDegrees);
    const auto Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("queueTicketId"), QueueTicketId);
    Body->SetStringField(TEXT("playerId"), Admission.PlayerId);
    Body->SetStringField(TEXT("sessionId"), Admission.SessionId);
    Body->SetStringField(TEXT("sessionEpoch"), LexToString(Admission.SessionEpoch));
    Body->SetObjectField(TEXT("spawn"), Location);
    FString Payload;
    if (!FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Payload)))
    {
        return nullptr;
    }

    const auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BaseUrl + TEXT("/internal/v1/gameservers/village-queue-origin"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    Request->SetHeader(TEXT("X-Game-Server-Id"), Admission.ServerInstanceId);
    Request->SetHeader(TEXT("X-Game-Server-Boot-Id"), Admission.ServerBootId);
    Request->SetContentAsString(Payload);
    Request->SetTimeout(5.0f);
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    return Request;
}

bool TryRestore(ACharacter& Pawn, const FGamePlatformVerifiedReturnSpawn& Spawn)
{
    UWorld* World = Pawn.GetWorld();
    const auto* Capsule = Pawn.GetCapsuleComponent();
    auto* Movement = Pawn.GetCharacterMovement();
    if (!Pawn.HasAuthority() || !World || World->bIsTearingDown || !Capsule || !Capsule->IsQueryCollisionEnabled() ||
        !Movement || Spawn.MapPackage != World->GetOutermost()->GetName() || Spawn.LocationCm.ContainsNaN() ||
        Spawn.LocationCm.GetAbsMax() > 100000000.0 || !FMath::IsFinite(Spawn.YawDegrees) || FMath::Abs(Spawn.YawDegrees) > 180.0)
    {
        return false;
    }

    const FCollisionQueryParams Query(SCENE_QUERY_STAT(VillageQueueReturn), false, &Pawn);
    const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
    const auto Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    if (World->OverlapBlockingTestByChannel(Spawn.LocationCm, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Query, Response))
    {
        return false;
    }

    FFindFloorResult Floor;
    Movement->FindFloor(Spawn.LocationCm, Floor, false);
    if (!Floor.IsWalkableFloor() || Floor.FloorDist > UCharacterMovementComponent::MAX_FLOOR_DIST || Cast<APawn>(Floor.HitResult.GetActor()))
    {
        return false;
    }

    // 当前胶囊已经通过精确位置检查，不让引擎自动挪到邻近位置。
    return Pawn.TeleportTo(Spawn.LocationCm, FRotator(0.0, Spawn.YawDegrees, 0.0), false, true);
}
}
