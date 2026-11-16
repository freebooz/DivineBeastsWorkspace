// 平台值能力人工台：参数源是隔离夹具；没有权威写入或复制业务算法。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Types/GamePlatformVersionRange.h"
#include "Types/GamePlatformErrorCode.h"
#include "Types/GamePlatformEquipmentTypes.h"
#include "Queries/GamePlatformEntitlementQuery.h"
#include "Definitions/GamePlatformProgressionTrackDefinition.h"
#include "Buffer/GamePlatformActionInputBuffer.h"
#include "GameplayTagsManager.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo
{
FGamePlatformResult EvaluateManualCoreVersion(const FManualValues& V)
{
    if(V.Num()!=9) return ManualError(TEXT("Demo.VersionParameters"),TEXT("版本需要三个三段数值版本。"));
    FGamePlatformVersion Versions[3];
    for(int32 I=0;I<9;++I)
    {
        if(V[I].Type!=EDivineBeastsDemoParameterType::Integer || V[I].IntegerValue<0 || V[I].IntegerValue>MAX_int32)
            return ManualError(TEXT("Demo.VersionParameters"),TEXT("版本分量必须为0..INT32_MAX整数，不夹紧。"));
        int32* Field=I%3==0?&Versions[I/3].Major:I%3==1?&Versions[I/3].Minor:&Versions[I/3].Patch;
        *Field=static_cast<int32>(V[I].IntegerValue);
    }
    FGamePlatformVersion Parsed;
    if(!FGamePlatformVersion::TryParse(Versions[0].ToString(),Parsed))
        return ManualError(TEXT("Demo.VersionParse"),TEXT("平台版本规范串无法回读。"));
    const auto Range=FGamePlatformVersionRange::Inclusive(Versions[1],Versions[2]);
    if(!Range.IsValid()) return ManualError(TEXT("Demo.VersionRange"),TEXT("平台拒绝逆序或非法范围。"));
    return ManualObserved(FString::Printf(TEXT("版本=%s；比较最低=%d；闭区间=%s；包含=%d"),
        *Parsed.ToString(),Parsed.Compare(Versions[1]),*Range.ToString(),Range.Contains(Parsed)));
}
class FPlatformValueScenario final : public FPlatformManualBase
{
public:
    FPlatformValueScenario(UWorld& W,FName F):FPlatformManualBase(W,F){}
    ~FPlatformValueScenario() override {CleanupService();}
private:
    FGamePlatformResult CleanupService() override {Buffer.Reset();return FGamePlatformResult::Success();}
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        FGamePlatformResult R=ManualError(TEXT("Demo.ValueOperation"),TEXT("该值能力没有此操作。"));
        if(Feature==TEXT("GamePlatformCore.02") && Op==TEXT("EvaluateResult"))
        {
            FGamePlatformErrorCode E;
            if(!FGamePlatformErrorCode::TryCreate(A.Id(TEXT("Domain")).ToString(),A.Id(TEXT("Code")).ToString(),E))
                R=ManualError(TEXT("Demo.InvalidErrorCode"),TEXT("平台拒绝错误域或名称。"));
            else
            {
                FGamePlatformErrorCode Parsed;
                if(!FGamePlatformErrorCode::TryParse(E.ToString(),Parsed)) R=ManualError(TEXT("Demo.ErrorRoundTrip"),TEXT("平台错误码回读失败。"));
                else switch(A.Int(TEXT("Status")))
                {
                    case 0:R=FGamePlatformResult::Success();R.Message=TEXT("真实Success工厂值。");break;
                    case 1:R=FGamePlatformResult::Failure(Parsed,TEXT("人工夹具指定失败值；不改变业务。"));break;
                    case 2:R=FGamePlatformResult::Cancelled(TEXT("真实Cancelled结果值；不是取消业务命令。"));break;
                    case 3:R=FGamePlatformResult::Unsupported(Parsed,TEXT("人工夹具指定Unsupported结果值。"));break;
                    default:R=ManualError(TEXT("Demo.ResultStatus"),TEXT("状态选择必须为0..3。"));
                }
            }
        }
        else if(Feature==TEXT("GamePlatformCore.03") && Op==TEXT("CompareVersions"))
        {
            FManualValues V;
            for(const TCHAR* Key:{TEXT("Major"),TEXT("Minor"),TEXT("Patch"),TEXT("MinMajor"),TEXT("MinMinor"),TEXT("MinPatch"),TEXT("MaxMajor"),TEXT("MaxMinor"),TEXT("MaxPatch")})
            {FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::Integer;X.IntegerValue=A.Int(Key);V.Add(X);}
            R=EvaluateManualCoreVersion(V);
        }
        else if(Feature==TEXT("GamePlatformProgression.01") && Op==TEXT("PreviewCurve"))
        {
            TStrongObjectPtr<UGamePlatformProgressionTrackDefinition> D(NewObject<UGamePlatformProgressionTrackDefinition>());
            D->ProgressionTrackId=TEXT("ManualTrack");D->MinLevel=1;D->MaxLevel=3;
            D->CumulativeXPThresholds={0,A.Int(TEXT("ThresholdTwo")),A.Int(TEXT("ThresholdThree"))};
            FString Why;
            if(!D->Validate(Why)) R=FGamePlatformResult::Failure(TEXT("Demo.InvalidCurve"),Why);
            else {const int64 XP=A.Int(TEXT("XP"));R=ManualObserved(FString::Printf(TEXT("纯预览：Level=%d；级内XP=%lld；下一等级需求=%lld；最大XP=%lld"),
                D->CalculateLevel(XP),D->CalculateXPIntoLevel(XP),D->CalculateXPForNextLevel(XP),D->GetMaxTotalXP()));}
        }
        else if(Feature==TEXT("GamePlatformEquipment.01") && Op==TEXT("ValidateSnapshot"))
        {
            FGamePlatformEquipmentSnapshot S;S.CharacterId=A.Id(TEXT("Character")).ToString();S.EquipmentRevision=A.Int(TEXT("Revision"));
            FGamePlatformEquipmentSlotState Slot;Slot.SlotId=A.Id(TEXT("Slot"));Slot.ItemInstanceId=TEXT("ManualItem");
            Slot.ItemDefinitionId=TEXT("ManualItemDefinition");Slot.EquipmentDefinitionId=TEXT("ManualEquipment");Slot.Revision=1;S.Slots.Add(Slot);
            if(A.Bool(TEXT("DuplicateSlot"))) S.Slots.Add(Slot);
            FString Why;
            if(!ValidateGamePlatformEquipmentSnapshot(S,Why)) R=FGamePlatformResult::Failure(TEXT("Demo.InvalidEquipmentSnapshot"),Why);
            else
            {
                FGamePlatformPublicEquipmentSnapshot Public;Public.PublicStateRevision=S.EquipmentRevision;
                FGamePlatformPublicEquipmentSlotState P;P.SlotId=Slot.SlotId;P.EquipmentDefinitionId=Slot.EquipmentDefinitionId;Public.Slots.Add(P);
                R=ValidateGamePlatformPublicEquipmentSnapshot(Public,Why)?ManualObserved(TEXT("真实Owner/Public纯值校验通过；未注册服务器装备定义，未穿戴。")):
                    FGamePlatformResult::Failure(TEXT("Demo.InvalidPublicEquipmentSnapshot"),Why);
            }
        }
        else if(Feature==TEXT("GamePlatformEntitlement.02") && Op==TEXT("QueryEntitlements"))
        {
            FGamePlatformEntitlementSnapshot S;S.Revision=1;S.GeneratedAtUtc=FDateTime::UtcNow();
            FGamePlatformEntitlementEntry E;E.EntitlementId=A.Id(TEXT("Entitlement"));E.TargetType=EGamePlatformEntitlementTargetType::Hero;
            E.TargetId=A.Id(TEXT("Target"));E.Status=A.Bool(TEXT("Active"))?EGamePlatformEntitlementStatus::Active:EGamePlatformEntitlementStatus::Revoked;S.Entitlements.Add(E);
            const TArray<FName> Keys={A.Id(TEXT("Query")),A.Id(TEXT("OtherQuery"))};
            R=ManualObserved(FString::Printf(TEXT("隔离快照查询：Has=%d；Any=%d；All=%d；HeroTarget=%d；不授予权益"),
                FGamePlatformEntitlementQuery::HasEntitlement(S,Keys[0]),FGamePlatformEntitlementQuery::HasAny(S,Keys),
                FGamePlatformEntitlementQuery::HasAll(S,Keys),FGamePlatformEntitlementQuery::HasTarget(S,EGamePlatformEntitlementTargetType::Hero,E.TargetId)));
        }
        else if(Feature==TEXT("GamePlatformInput.06"))
        {
            if(Op==TEXT("EnqueueInput"))
            {
                const int64 Sequence=A.Int(TEXT("Sequence")),Generation=A.Int(TEXT("BindingGeneration"));
                if(Sequence<=0 || Generation<=0) R=ManualError(TEXT("Demo.InputEventIdentity"),TEXT("输入序号与代次必须为正。"));
                else
                {
                    FGamePlatformInputEvent E;E.SemanticId.Tag=UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Input.Confirm"),false);
                    E.Phase=ETriggerEvent::Started;E.BindingGeneration=static_cast<uint64>(Generation);E.Sequence=static_cast<uint64>(Sequence);
                    R=Buffer.Enqueue(E,A.Number(TEXT("NowSeconds")))?ManualObserved(FString::Printf(TEXT("真实缓冲入队；Num=%d"),Buffer.Num())):
                        ManualError(TEXT("Demo.InputBufferRejected"),TEXT("平台拒绝事件身份、重复序号、时间或未登记语义。"));
                }
            }
            else if(Op==TEXT("ConsumeInput"))
            {
                TArray<FGamePlatformInputEvent> Out;
                const int32 N=Buffer.ConsumePending(A.Number(TEXT("NowSeconds")),static_cast<uint64>(A.Int(TEXT("BindingGeneration"))),Out);
                R=ManualObserved(FString::Printf(TEXT("真实缓冲消费=%d；剩余=%d；不发起Gameplay"),N,Buffer.Num()));
            }
            else if(Op==TEXT("ResetInput")){Buffer.Reset();R=ManualObserved(TEXT("仅本案例缓冲已清空。"));}
        }
        Done(Context.Token,R);
    }
    FGamePlatformActionInputBuffer Buffer;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformValueScenario(UWorld& W,FName F)
{return MakeUnique<FPlatformValueScenario>(W,F);}
}

