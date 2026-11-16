// Surface世界服务的有界压雪高度场：Data拥有材质租约，World拥有两张瞬态RT和两个MID。
// R=压实率，G=柔和雪唇。无CPU像素读回；只在足迹存在期间运行5Hz回填，接触按帧合批。
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Scalability.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceSnowContact,Log,All);
namespace
{
    constexpr int32 MaxSnowBatch=16;
    const FName FieldName(TEXT("SnowContactTexture"));
    const FName OriginName(TEXT("SnowContactOriginExtent"));
    float ExtentCm() { return FMath::Clamp(GetDefault<UGamePlatformSurfaceSettings>()->SnowContactExtentCm,800.f,6400.f); }
    FVector2D ViewCenter(UWorld& World,const FVector& Fallback)
    {
        if (auto* PC=World.GetFirstPlayerController())
            if (APawn* Pawn=PC->GetPawn()) return FVector2D(Pawn->GetActorLocation());
        return FVector2D(Fallback);
    }
}

bool UGamePlatformSurfaceWorldSubsystem::RegisterSnowContactMaterial(UMaterialInstanceDynamic& Material)
{
    check(IsInGameThread());
    if (bClosing || !GetDefault<UGamePlatformSurfaceSettings>()->bEnableSnowContacts) return false;
    // 只登记具有明确参数合同的接收材质，避免无意覆盖其他世界的材质。
    UTexture* DefaultTexture=nullptr;
    if (!Material.GetTextureParameterValue(FMaterialParameterInfo(FieldName),DefaultTexture)) return false;
    if (SnowContactReceivers.Contains(&Material)) return true;
    if (SnowContactReceivers.Num() >= 512) return false;
    SnowContactReceivers.Add(&Material);
    BeginSnowContactLoad();
    if (SnowContactField) BindSnowContactReceivers();
    return true;
}

bool UGamePlatformSurfaceWorldSubsystem::BeginSnowContactLoad()
{
    if (bClosing || bSnowContactLoadAttempted) return SnowContactUpdateMID != nullptr;
    auto* World=GetWorld(); auto* GI=World?World->GetGameInstance():nullptr;
    auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr;
    const auto* Settings=GetDefault<UGamePlatformSurfaceSettings>();
    if (!Settings->bEnableSnowContacts || !Settings->SnowContactUpdateMaterial.ToSoftObjectPath().IsValid() || !Data) return false;
    bSnowContactLoadAttempted=true;
    const int64 Generation=++SnowContactGeneration;
    const TWeakObjectPtr<UGamePlatformSurfaceWorldSubsystem> Weak(this);
    FGamePlatformResult Accepted;
    SnowContactLease=Data->AcquireResources({Settings->SnowContactUpdateMaterial.ToSoftObjectPath()},EGamePlatformDataLifetime::World,this,
        [Weak,Generation](const FGamePlatformDataLease& Lease,const FGamePlatformResult& Result)
        { if (auto* Self=Weak.Get()) Self->OnSnowContactMaterialLoaded(Generation,Lease,Result); },Accepted);
    return Accepted.IsSuccess() && SnowContactLease.IsValid();
}

void UGamePlatformSurfaceWorldSubsystem::OnSnowContactMaterialLoaded(int64 Generation,const FGamePlatformDataLease& Lease,const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (bClosing || Generation!=SnowContactGeneration || !Lease.IsValid() || Lease.LeaseId!=SnowContactLease.LeaseId ||
        Lease.Generation!=SnowContactLease.Generation || Lease.ScopeId!=SnowContactLease.ScopeId ||
        Lease.IssuerProof!=SnowContactLease.IssuerProof || Lease.ResourcePaths!=SnowContactLease.ResourcePaths) return;
    auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr; auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr;
    auto* Material=Lease.ResourcePaths.Num()==1?Cast<UMaterialInterface>(Lease.ResourcePaths[0].ResolveObject()):nullptr;
    if (!Result.IsSuccess() || !Data || Data->GetLeaseState(Lease)!=EGamePlatformDataRequestState::Succeeded || !Material)
    {
        UE_LOG(LogSurfaceSnowContact,Warning,TEXT("GP_SNOW_CONTACT_RESOURCE_FAILED：保留原雪面，压痕未启用"));
        PendingSnowContacts.Reset(); if (Data) Data->ReleaseResources(Lease); SnowContactLease={}; return;
    }
    SnowContactUpdateMID=UMaterialInstanceDynamic::Create(Material,this);
    SnowContactCopyMID=UMaterialInstanceDynamic::Create(Material,this);
    // 异步加载不重播已经过去的足迹；后续真实触地事件重新入队。
    PendingSnowContacts.Reset();
    UE_LOG(LogSurfaceSnowContact,Display,TEXT("GP_SNOW_CONTACT_RESOURCE_READY"));
}

bool UGamePlatformSurfaceWorldSubsystem::SubmitSnowContact(const FGamePlatformSnowContact& Contact)
{
    check(IsInGameThread());
    auto* World=GetWorld();
    if (bClosing || !World || World->bIsTearingDown || !Contact.IsValid() || CurrentState.GlobalSnowAmount<.01f ||
        !GetDefault<UGamePlatformSurfaceSettings>()->bEnableSnowContacts || SnowContactRecentIds.Contains(Contact.EventId)) return false;
    if (!SnowContactUpdateMID) { BeginSnowContactLoad(); return false; }
    // 只保留本地观看者附近的表面；不为地图远端的装饰分配纹理。
    if (FVector2D::Distance(ViewCenter(*World,Contact.Location),FVector2D(Contact.Location))>ExtentCm()*.45f) return false;
    if (PendingSnowContacts.Num()>=MaxSnowBatch) return false;
    PendingSnowContacts.Add(Contact); SnowContactRecentIds.Add(Contact.EventId);
    if (SnowContactRecentIds.Num()>256) SnowContactRecentIds.RemoveAt(0,SnowContactRecentIds.Num()-256,EAllowShrinking::No);
    if (!bSnowContactFlushQueued)
    {
        bSnowContactFlushQueued=true;
        SnowContactFlushTimer=World->GetTimerManager().SetTimerForNextTick(this,&ThisClass::FlushSnowContacts);
    }
    return true;
}

void UGamePlatformSurfaceWorldSubsystem::BindSnowContactReceivers()
{
    const float Extent=ExtentCm();
    SnowContactReceivers.RemoveAll([](const auto& Receiver){return !Receiver.IsValid();});
    int32 Verified=0;
    for (const auto& Receiver:SnowContactReceivers)
    {
        auto* Mid=Receiver.Get();
        Mid->SetTextureParameterValue(FieldName,SnowContactField);
        Mid->SetVectorParameterValue(OriginName,FLinearColor(SnowContactOrigin.X,SnowContactOrigin.Y,1.f/Extent,1.f));
        if (Mid->K2_GetTextureParameterValue(FieldName)==SnowContactField) ++Verified;
    }
    UE_LOG(LogSurfaceSnowContact,Display,TEXT("GP_SNOW_CONTACT_RECEIVERS_BOUND verified=%d total=%d"),Verified,SnowContactReceivers.Num());
}

void UGamePlatformSurfaceWorldSubsystem::FlushSnowContacts()
{
    check(IsInGameThread());
    bSnowContactFlushQueued=false;
    auto* World=GetWorld();
    if (bClosing || !World || World->bIsTearingDown || !SnowContactUpdateMID || CurrentState.GlobalSnowAmount<.001f) { ClearSnowContacts(); return; }
    const double Now=World->GetTimeSeconds(); const float Extent=ExtentCm();
    const bool bHasNew=!PendingSnowContacts.IsEmpty();
    if (!SnowContactField && !bHasNew) return;
    FVector2D PreviousOrigin=SnowContactOrigin;
    const FVector Fallback=bHasNew?PendingSnowContacts.Last().Location:FVector(SnowContactOrigin.X+Extent*.5f,SnowContactOrigin.Y+Extent*.5f,0);
    const FVector2D Center=ViewCenter(*World,Fallback);
    const FVector2D WantedOrigin(FMath::GridSnap(Center.X,Extent*.25f)-Extent*.5f,FMath::GridSnap(Center.Y,Extent*.25f)-Extent*.5f);
    bool bRebind=false;
    if (!SnowContactField)
    {
        const int32 Quality=Scalability::GetQualityLevels().EffectsQuality;
        const int32 Size=FMath::Clamp(GetDefault<UGamePlatformSurfaceSettings>()->SnowContactResolution,256,1024) / (Quality<=0?4:Quality==1?2:1);
        SnowContactField=UKismetRenderingLibrary::CreateRenderTarget2D(this,Size,Size,RTF_RG16f,FLinearColor::Black,false);
        SnowContactScratch=UKismetRenderingLibrary::CreateRenderTarget2D(this,Size,Size,RTF_RG16f,FLinearColor::Black,false);
        if (!SnowContactField || !SnowContactScratch) { UE_LOG(LogSurfaceSnowContact,Error,TEXT("GP_SNOW_CONTACT_RT_FAILED")); return; }
        SnowContactOrigin=WantedOrigin; PreviousOrigin=WantedOrigin; SnowContactLastUpdate=Now; bRebind=true;
        UE_LOG(LogSurfaceSnowContact,Display,TEXT("GP_SNOW_CONTACT_RT_READY size=%d extent_cm=%.0f bytes=%lld"),Size,Extent,static_cast<int64>(Size)*Size*8);
    }
    if (bHasNew && !SnowContactOrigin.Equals(WantedOrigin,.1f)) { SnowContactOrigin=WantedOrigin; bRebind=true; }
    const FVector2D Offset=(SnowContactOrigin-PreviousOrigin)/Extent;
    const float FillPerSecond=1.f/120.f+.025f*CurrentState.SnowIntensity;
    SnowContactUpdateMID->SetTextureParameterValue(TEXT("PreviousField"),SnowContactField);
    SnowContactUpdateMID->SetVectorParameterValue(TEXT("PreviousOffset"),FLinearColor(Offset.X,Offset.Y,0,0));
    SnowContactUpdateMID->SetScalarParameterValue(TEXT("DecayAmount"),FMath::Max(0.f,static_cast<float>(Now-SnowContactLastUpdate))*FillPerSecond);
    SnowContactUpdateMID->SetScalarParameterValue(TEXT("ContactCount"),PendingSnowContacts.Num());
    for (int32 Index=0;Index<PendingSnowContacts.Num();++Index)
    {
        const auto& Contact=PendingSnowContacts[Index]; const FVector Dir=Contact.Forward.GetSafeNormal2D();
        const FVector2D UV=(FVector2D(Contact.Location)-SnowContactOrigin)/Extent;
        SnowContactUpdateMID->SetVectorParameterValue(FName(*FString::Printf(TEXT("ContactA%d"),Index)),FLinearColor(UV.X,UV.Y,Dir.X,Dir.Y));
        SnowContactUpdateMID->SetVectorParameterValue(FName(*FString::Printf(TEXT("ContactB%d"),Index)),FLinearColor(Contact.LengthCm*.5f/Extent,Contact.WidthCm*.5f/Extent,Contact.Pressure,0));
    }
    // 两个独立MID防止异步渲染读取到下一次设置；输出纹理地址固定，不逐帧重绑64个接收MID。
    UKismetRenderingLibrary::DrawMaterialToRenderTarget(this,SnowContactScratch,SnowContactUpdateMID);
    SnowContactCopyMID->SetTextureParameterValue(TEXT("PreviousField"),SnowContactScratch);
    SnowContactCopyMID->SetScalarParameterValue(TEXT("ContactCount"),0.f);
    SnowContactCopyMID->SetScalarParameterValue(TEXT("DecayAmount"),0.f);
    UKismetRenderingLibrary::DrawMaterialToRenderTarget(this,SnowContactField,SnowContactCopyMID);
    if (bRebind) BindSnowContactReceivers();
    SnowContactLastUpdate=Now;
    if (bHasNew)
    {
        SnowContactLastStamp=Now; SnowContactSubmitted+=PendingSnowContacts.Num();
        UE_LOG(LogSurfaceSnowContact,Verbose,TEXT("GP_SNOW_CONTACT_STAMP batch=%d total=%d"),PendingSnowContacts.Num(),SnowContactSubmitted);
        if (!World->GetTimerManager().IsTimerActive(SnowContactDecayTimer)) World->GetTimerManager().SetTimer(SnowContactDecayTimer,this,&ThisClass::FlushSnowContacts,.2f,true);
    }
    PendingSnowContacts.Reset();
    if (Now-SnowContactLastStamp>121.) ClearSnowContacts();
}

void UGamePlatformSurfaceWorldSubsystem::ClearSnowContacts()
{
    PendingSnowContacts.Reset(); bSnowContactFlushQueued=false;
    if (auto* World=GetWorld()) { World->GetTimerManager().ClearTimer(SnowContactDecayTimer); World->GetTimerManager().ClearTimer(SnowContactFlushTimer); }
    if (SnowContactField) UKismetRenderingLibrary::ClearRenderTarget2D(this,SnowContactField,FLinearColor::Black);
    if (SnowContactScratch) UKismetRenderingLibrary::ClearRenderTarget2D(this,SnowContactScratch,FLinearColor::Black);
}
void UGamePlatformSurfaceWorldSubsystem::ReleaseSnowContacts()
{
    ++SnowContactGeneration; ClearSnowContacts();
    for (const auto& Receiver:SnowContactReceivers) if (auto* Mid=Receiver.Get()) Mid->SetTextureParameterValue(FieldName,nullptr);
    SnowContactReceivers.Reset(); SnowContactRecentIds.Reset();
    SnowContactUpdateMID=nullptr; SnowContactCopyMID=nullptr;
    if (SnowContactField) SnowContactField->ReleaseResource();
    if (SnowContactScratch) SnowContactScratch->ReleaseResource();
    SnowContactField=nullptr; SnowContactScratch=nullptr;
    const auto Lease=SnowContactLease; SnowContactLease={};
    auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
    if (auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr; Data && Lease.IsValid()) Data->ReleaseResources(Lease);
}
