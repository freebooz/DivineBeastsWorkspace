#include "Validators/GamePlatformPCGWorldValidator.h"

#include "Actors/GamePlatformPCGActors.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

namespace
{
/**
 * 先识别真实PCG世界内容：空天气/UI/角色预览图没有PCG Actor时应直接跳过本校验器。
 * DataValidation已调用CanValidateAsset之后若又返回NotValidated，会在UE5.8产生日志错误。
 * WorldPartition未加载的PCG描述符也属于必须校验的范围，禁止因只看已加载Actor而放过。
 */
bool HasAnyPCGWorldContent(UWorld* World)
{
    if (!IsValid(World)) { return false; }
    for (TActorIterator<AGamePlatformPCGActorBase> It(World); It; ++It)
    {
        if (IsValid(*It)) { return true; }
    }
    for (TActorIterator<AGamePlatformPCGWorldDirector> It(World); It; ++It)
    {
        if (IsValid(*It)) { return true; }
    }
    if (UWorldPartition* Partition = World->GetWorldPartition())
    {
        bool bHasRelevantDescriptor = false;
        const auto FindOne = [&bHasRelevantDescriptor](const FWorldPartitionActorDescInstance*)
        {
            bHasRelevantDescriptor = true;
            return false;
        };
        FWorldPartitionHelpers::ForEachActorDescInstance(
            Partition, AGamePlatformPCGActorBase::StaticClass(), FindOne);
        if (!bHasRelevantDescriptor)
        {
            FWorldPartitionHelpers::ForEachActorDescInstance(
                Partition, AGamePlatformPCGWorldDirector::StaticClass(), FindOne);
        }
        return bHasRelevantDescriptor;
    }
    return false;
}
}


bool UGamePlatformPCGWorldValidator::CanValidateAsset_Implementation(
    const FAssetData&,
    UObject* InObject,
    FDataValidationContext&) const
{
    return HasAnyPCGWorldContent(Cast<UWorld>(InObject));
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


    // World Partition 的未加载External Actor不在TActorIterator中：仅扫描已加载对象不能证明全图完整。
    // 使用UE5.8官方ActorDesc API审计未加载的PCG放置器/Director；若发现未加载关键参与者，
    // 拒绝将当前局部地图标为“验证通过”，要求地编加载相关分区后再执行正式校验。
    if (UWorldPartition* Partition = World->GetWorldPartition())
    {
        int32 DescribedParticipants = 0;
        int32 DescribedDirectors = 0;
        TSet<FGuid> DescribedGuids;
        bool bDuplicateGuid = false;

        const auto Scan = [&](UClass* Class, int32& Count)
        {
            FWorldPartitionHelpers::ForEachActorDescInstance(Partition, Class,
                [&](const FWorldPartitionActorDescInstance* Descriptor)
                {
                    if (!Descriptor || !Descriptor->GetGuid().IsValid() ||
                        DescribedGuids.Contains(Descriptor->GetGuid()))
                    {
                        bDuplicateGuid = true;
                        return true;
                    }
                    DescribedGuids.Add(Descriptor->GetGuid());
                    ++Count;
                    return true;
                });
        };
        Scan(AGamePlatformPCGActorBase::StaticClass(), DescribedParticipants);
        Scan(AGamePlatformPCGWorldDirector::StaticClass(), DescribedDirectors);

        if (bDuplicateGuid || DescribedParticipants > ParticipantsInWorld.Num() ||
            DescribedDirectors > Directors.Num())
        {
            AssetFails(InAsset, FText::FromString(TEXT(
                "World Partition存在未加载/重复的PCG Actor描述符；请加载关联分区并重新验证完整Director与来源身份。")));
            return EDataValidationResult::Invalid;
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

    // 注册集合合法不代表空间掩码/阶段可执行；读取完整数值快照来拒绝非法Polygon、超长Spline和未批准阶段。
    TArray<FGamePlatformPCGSpatialMask> SpatialMasks;
    if (!Director->CollectSpatialMasks(SpatialMasks, DirectorError))
    {
        AssetFails(InAsset, FText::FromString(TEXT("PCG空间掩码校验失败：") + DirectorError));
        return EDataValidationResult::Invalid;
    }
    TArray<AGamePlatformPCGActorBase*> StaticPlan;
    if (!Director->BuildStaticExecutionPlan(StaticPlan, DirectorError))
    {
        AssetFails(InAsset, FText::FromString(TEXT("PCG静态阶段/图合同非法：") + DirectorError));
        return EDataValidationResult::Invalid;
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
