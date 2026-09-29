#include "Validators/GamePlatformPCGWorldValidator.h"

#include "Actors/GamePlatformPCGActors.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UGamePlatformPCGWorldValidator::CanValidateAsset_Implementation(
    const FAssetData&,
    UObject* InObject,
    FDataValidationContext&) const
{
    return InObject && InObject->IsA<UWorld>();
}

EDataValidationResult UGamePlatformPCGWorldValidator::ValidateLoadedAsset_Implementation(
    const FAssetData&,
    UObject* InAsset,
    FDataValidationContext&)
{
    check(IsInGameThread());
    UWorld* World = Cast<UWorld>(InAsset);
    if (!World)
    {
        return EDataValidationResult::NotValidated;
    }

    TArray<AGamePlatformPCGActorBase*> ParticipantsInWorld;
    for (TActorIterator<AGamePlatformPCGActorBase> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            ParticipantsInWorld.Add(*It);
        }
    }

    TArray<AGamePlatformPCGWorldDirector*> Directors;
    for (TActorIterator<AGamePlatformPCGWorldDirector> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            Directors.Add(*It);
        }
    }

    if (ParticipantsInWorld.IsEmpty() && Directors.IsEmpty())
    {
        return EDataValidationResult::NotValidated;
    }

    if (Directors.Num() != 1)
    {
        AssetFails(
            InAsset,
            FText::FromString(FString::Printf(
                TEXT("PCG地图必须恰好存在一个AGamePlatformPCGWorldDirector；当前数量=%d。"),
                Directors.Num())));
        return EDataValidationResult::Invalid;
    }

    AGamePlatformPCGWorldDirector* Director = Directors[0];
    FString DirectorError;
    if (!Director->ValidateParticipantSet(DirectorError))
    {
        AssetFails(InAsset, FText::FromString(TEXT("PCG WorldDirector参与者集合非法：") + DirectorError));
        return EDataValidationResult::Invalid;
    }

    TSet<const AGamePlatformPCGActorBase*> Registered;
    for (uint8 StageValue = static_cast<uint8>(EGamePlatformPCGWorldStage::FieldRead);
         StageValue <= static_cast<uint8>(EGamePlatformPCGWorldStage::RuntimeDetail);
         ++StageValue)
    {
        for (AGamePlatformPCGActorBase* Participant : Director->GetParticipantsForStage(
                 static_cast<EGamePlatformPCGWorldStage>(StageValue)))
        {
            if (IsValid(Participant))
            {
                Registered.Add(Participant);
            }
        }
    }

    for (const AGamePlatformPCGActorBase* Participant : ParticipantsInWorld)
    {
        if (!Registered.Contains(Participant))
        {
            AssetFails(
                InAsset,
                FText::FromString(FString::Printf(
                    TEXT("PCG放置器未注册到唯一WorldDirector：%s"),
                    *GetNameSafe(Participant))));
            return EDataValidationResult::Invalid;
        }
    }

    if (Registered.Num() != ParticipantsInWorld.Num())
    {
        AssetFails(InAsset, FText::FromString(TEXT("WorldDirector注册集合与地图真实PCG放置器数量不一致。")));
        return EDataValidationResult::Invalid;
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
