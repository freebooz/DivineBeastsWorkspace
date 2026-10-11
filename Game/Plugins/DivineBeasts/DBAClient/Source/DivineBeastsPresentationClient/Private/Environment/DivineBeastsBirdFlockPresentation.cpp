// 神兽联盟项目层鸟群表现编排；只提交中立语义请求和复用既有内容包/Data租约。
// 平台GamePlatformVFX是唯一Niagara执行器；此处无逐帧更新、逐鸟Actor或服务器复制。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

bool UDivineBeastsPresentationClientSubsystem::StartAmbientBirdFlock(
    FName FlockId,FName SpeciesId,int32 BirdCount,const FVector& WorldOrigin,FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    if(FlockId.IsNone() || BirdCount<0 || BirdCount>512 || WorldOrigin.ContainsNaN())
    {
        OutError=TEXT("鸟群标识不能为空；数量须0..512，世界坐标须为有限值。"); return false;
    }
    if(BirdCount==0) { StopAmbientBirdFlock(FlockId); return true; }
    UWorld* World=GetLocalPlayer()?GetLocalPlayer()->GetWorld():nullptr;
    if(bClosing || !World || World->bIsTearingDown || World->GetNetMode()==NM_DedicatedServer || !PlatformPresentation)
    {
        OutError=TEXT("本地鸟群表现世界尚未就绪或已关闭。"); return false;
    }
    if(!FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(SpeciesId).IsValid())
    {
        OutError=TEXT("鸟种无效：允许Seagull（海鸥）、Swallow（燕子）、Eagle（雄鹰）。"); return false;
    }
    if(!AmbientBirdFlocks.Contains(FlockId) && AmbientBirdFlocks.Num()>=16)
    {
        OutError=TEXT("单个本地玩家世界最多16个鸟群请求，防止无界场景请求。"); return false;
    }
    if(auto* Existing=AmbientBirdFlocks.Find(FlockId))
    {
        if(Existing->SpeciesId==SpeciesId && Existing->Count==BirdCount && Existing->Origin.Equals(WorldOrigin))
        {
            if(BirdFlockPack.IsValid())
            {
                FString StateError;
                const auto State=GetContentPackState(BirdFlockPack,StateError);
                if(State==EDivineBeastsPresentationContentPackState::Loading || State==EDivineBeastsPresentationContentPackState::Active)
                { SubmitPendingBirdFlocks(); return true; }
            }
        }
        // 数量改变需要重新Burst；先撤销旧请求，但同一共享资源租约继续复用。
        const auto Previous=*Existing;
        Existing->RequestId.Invalidate();
        CancelBirdFlockRequest(Previous.RequestId,Previous.SpeciesId,FlockId);
    }
    FBirdFlockRequestState Desired;
    Desired.SpeciesId=SpeciesId; Desired.Count=BirdCount; Desired.Origin=WorldOrigin;
    AmbientBirdFlocks.Add(FlockId,Desired);
    bool bNeedPack=true;
    if(BirdFlockPack.IsValid())
    {
        FString StateError;
        const auto State=GetContentPackState(BirdFlockPack,StateError);
        bNeedPack=State!=EDivineBeastsPresentationContentPackState::Loading && State!=EDivineBeastsPresentationContentPackState::Active;
    }
    if(bNeedPack)
    {
        if(BirdFlockPack.IsValid()) DeactivateContentPack(BirdFlockPack);
        BirdFlockPack={};
        FDivineBeastsPresentationContentPackFragment Fragment;
        Fragment.ContentPackId=TEXT("DBA.Environment.BirdFlock");
        Fragment.Revision=1; Fragment.bRequiredPreload=true;
        Fragment.LifecycleScope=EGamePlatformPresentationContextScope::World;
        Fragment.CatalogFragment=FDivineBeastsPresentationProjectCatalog::BuildBirdFlockFragment();
        Fragment.CatalogFragment.Scope=EGamePlatformPresentationCatalogScope::ContentPack;
        Fragment.CatalogFragment.OwnerScopeId=Fragment.ContentPackId;
        Fragment.CatalogFragment.LifecycleScope=Fragment.LifecycleScope;
        for(auto& Entry:Fragment.CatalogFragment.Entries) Entry.Scope=EGamePlatformPresentationCatalogScope::ContentPack;
        BirdFlockPack=ActivateContentPack(Fragment,OutError);
        if(!BirdFlockPack.IsValid()) { AmbientBirdFlocks.Remove(FlockId); return false; }
    }
    // 补偿缓存命中时预载可能在ActivateContentPack返回前同步完成的时序。
    SubmitPendingBirdFlocks();
    return true;
}

void UDivineBeastsPresentationClientSubsystem::SubmitPendingBirdFlocks()
{
    if(bClosing || !BirdFlockPack.IsValid() || !PlatformPresentation) return;
    FString Error;
    if(GetContentPackState(BirdFlockPack,Error)!=EDivineBeastsPresentationContentPackState::Active) return;
    UWorld* World=GetLocalPlayer()?GetLocalPlayer()->GetWorld():nullptr;
    if(!World || World->bIsTearingDown || World->GetNetMode()==NM_DedicatedServer) return;
    // 提交可能触发同步回调，使用键快照而不跨回调持有TMap内部引用。
    TArray<FName> Keys; AmbientBirdFlocks.GenerateKeyArray(Keys);
    for(FName Key:Keys)
    {
        auto* State=AmbientBirdFlocks.Find(Key);
        if(!State || State->RequestId.IsValid() || State->Count<=0) continue;
        const auto Snapshot=*State;
        FGamePlatformPresentationRequest Request;
        Request.RequestId=FGuid::NewGuid();
        Request.SemanticTag=FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(Snapshot.SpeciesId);
        Request.ContextId=Key; Request.SourceLocation=Snapshot.Origin;
        Request.ImpactLocation=Snapshot.Origin; Request.ImpactNormal=FVector::UpVector;
        Request.Priority=EGamePlatformPresentationPriority::Low;
        Request.Lifetime=EGamePlatformPresentationLifetime::Persistent;
        Request.PredictionState=EGamePlatformPresentationPredictionState::Confirmed;
        Request.IntegerParameters.Add(TEXT("User.BirdCount"),Snapshot.Count);
        State->RequestId=Request.RequestId;
        const auto Result=SubmitProjectRequest(Request);
        if(Result!=EGamePlatformPresentationSubmitResult::Submitted)
        {
            if(auto* Current=AmbientBirdFlocks.Find(Key); Current && Current->RequestId==Request.RequestId)
                Current->RequestId.Invalidate();
            UE_LOG(LogTemp,Warning,TEXT("[BirdFlock] 请求未提交：%s，状态%d"),*Key.ToString(),static_cast<int32>(Result));
        }
        else
        {
            UE_LOG(LogTemp,Log,TEXT("[BirdFlock] 已提交 %s，物种%s，配置数量%d；GPU可见状态由平台诊断确认。"),
                *Key.ToString(),*Snapshot.SpeciesId.ToString(),Snapshot.Count);
        }
    }
}

void UDivineBeastsPresentationClientSubsystem::CancelBirdFlockRequest(const FGuid& RequestId,FName SpeciesId,FName FlockId)
{
    if(!RequestId.IsValid() || !PlatformPresentation) return;
    FGamePlatformPresentationRequest Cancel;
    Cancel.RequestId=RequestId;
    Cancel.ContextId=FlockId;
    Cancel.SemanticTag=FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(SpeciesId);
    Cancel.Priority=EGamePlatformPresentationPriority::Low;
    Cancel.Lifetime=EGamePlatformPresentationLifetime::Persistent;
    Cancel.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;
    SubmitProjectRequest(Cancel);
}

void UDivineBeastsPresentationClientSubsystem::StopAmbientBirdFlock(FName FlockId)
{
    check(IsInGameThread());
    FBirdFlockRequestState Removed;
    if(AmbientBirdFlocks.RemoveAndCopyValue(FlockId,Removed))
        CancelBirdFlockRequest(Removed.RequestId,Removed.SpeciesId,FlockId);
    if(AmbientBirdFlocks.IsEmpty() && BirdFlockPack.IsValid())
    {
        const auto Old=BirdFlockPack; BirdFlockPack={};
        DeactivateContentPack(Old);
    }
}

void UDivineBeastsPresentationClientSubsystem::StopAllAmbientBirdFlocks()
{
    check(IsInGameThread());
    const auto Pending=MoveTemp(AmbientBirdFlocks);
    AmbientBirdFlocks.Reset();
    for(const auto& Pair:Pending) CancelBirdFlockRequest(Pair.Value.RequestId,Pair.Value.SpeciesId,Pair.Key);
    const auto Old=BirdFlockPack; BirdFlockPack={};
    if(Old.IsValid()) DeactivateContentPack(Old);
}

int32 UDivineBeastsPresentationClientSubsystem::GetRequestedBirdCount(FName FlockId) const
{
    const auto* State=AmbientBirdFlocks.Find(FlockId);
    return State?State->Count:0;
}

void UDivineBeastsPresentationClientSubsystem::StartDefaultWorldBirdFlock(UWorld* World)
{
    if(!World || !GetLocalPlayer() || World!=GetLocalPlayer()->GetWorld() || World->GetNetMode()==NM_DedicatedServer) return;
    // 使用已知项目内容包归属和体验身份，不把任意竞技地图识别为开放世界。
    const FString Package=World->GetOutermost()->GetName();
    const bool bVillage=Package.StartsWith(TEXT("/DBAWorldPack_Village/")) || ProjectContext.ExperienceId.ToString().StartsWith(TEXT("Experience.Village."));
    const bool bOpenWorld=Package.StartsWith(TEXT("/DBAWorldPack_OpenWorld/")) || ProjectContext.WorldId.ToString().StartsWith(TEXT("World.OpenWorld."));
    if(!bVillage && !bOpenWorld) return;
    // 分屏共享同一世界，仅首个本地玩家自动建环境群，避免多份可见鸟群重复渲染。
    if(!World->GetGameInstance() || World->GetGameInstance()->GetFirstGamePlayer()!=GetLocalPlayer()) return;
    // 优先使用真实出生点的场景高度，避免硬编码世界零点导致鸟群埋入高海拔地形。
    // 这是客户端装饰锚点，不改变服务器出生/碰撞事实；生产区域由场景组件进一步配置。
    FVector Origin(0,0,1800);
    for(TActorIterator<APlayerStart> It(World);It;++It)
    {
        Origin=It->GetActorLocation()+FVector(0,0,1800);
        break;
    }
    FString Error;
    if(!StartAmbientBirdFlock(TEXT("DBA.Environment.WorldBirds"),bVillage?FName(TEXT("Seagull")):FName(TEXT("Eagle")),
        bVillage?24:5,Origin,Error))
        UE_LOG(LogTemp,Warning,TEXT("[BirdFlock] 默认世界鸟群尚未受理：%s"),*Error);
}
