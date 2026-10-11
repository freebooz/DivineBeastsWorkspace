// 神兽联盟客户端场景组件：Begin/EndPlay事件建立/撤销自有注册，不启动独立渲染器。
// 身份替换先验证候选再撤销旧绑定；同步完成通知可重入，必须核对本组件变更token。
#include "Environment/DivineBeastsFaunaSceneComponent.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "Interfaces/GamePlatformVFXService.h"

UDivineBeastsFaunaSceneComponent::UDivineBeastsFaunaSceneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

TArray<FString> UDivineBeastsFaunaSceneComponent::GetFaunaSpeciesOptions() const
{
    return {TEXT("ButterflyAzure"), TEXT("FishSilver")};
}

void UDivineBeastsFaunaSceneComponent::BeginPlay()
{
    bEndingPlay = false;
    Super::BeginPlay();
    FString Error;
    if (!ApplyFauna(Error) && !Error.IsEmpty())
        UE_LOG(LogTemp, Warning, TEXT("[Fauna] 场景锚点：%s"), *Error);
}

bool UDivineBeastsFaunaSceneComponent::ApplyFauna(FString& OutError)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsFaunaSceneComponent> KeepComponent(this);
    OutError.Reset();
    UWorld* World = GetWorld();
    if (bEndingPlay || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) return false;
    UGameInstance* GI = World->GetGameInstance();
    ULocalPlayer* Player = GI ? GI->GetFirstGamePlayer() : nullptr;
    auto* Service = Player ? Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>() : nullptr;
    if (!Service)
    {
        OutError = TEXT("本地玩家表现服务尚未建立。");
        return false;
    }

    const FGuid Operation = FGuid::NewGuid();
    ApplyMutationToken = Operation;
    const auto OldService = BoundPresentation;
    const FName OldId = BoundHabitatId;
    const FGuid OldToken = BoundRegistrationToken;
    const FName NewId = HabitatId.IsNone() ? FName(*GetPathName()) : HabitatId;
    const FGuid NewToken = OldService.Get() == Service && OldId == NewId && OldToken.IsValid()
        ? OldToken : FGuid::NewGuid();

    const int32 DesiredCount = Count;
    // 不先Stop旧绑定：改ID与非法数量/物种同时发生时，失败必须留下原来的合法群落。
    if (!Service->StartAmbientFaunaForOwner(
        NewId, SpeciesId, DesiredCount, Seed, GetComponentLocation(), this, NewToken, OutError)) return false;
    if (ApplyMutationToken != Operation || bEndingPlay)
    {
        // 重入操作已接管绑定。只清理无人认领的本次候选，不能撤销内层已经接纳的同一注册。
        if (BoundPresentation.Get() != Service || BoundHabitatId != NewId || BoundRegistrationToken != NewToken)
            Service->StopAmbientFaunaForOwner(NewId, NewToken);
        OutError = TEXT("场景群落配置已在同步通知中被更新或停止。");
        return false;
    }

    if (DesiredCount > 0)
    {
        BoundPresentation = Service;
        BoundHabitatId = NewId;
        BoundRegistrationToken = NewToken;
    }
    else
    {
        BoundPresentation.Reset(); BoundHabitatId = NAME_None; BoundRegistrationToken.Invalidate();
    }
    // 先发布新绑定再取消旧句柄；旧取消通知若重入Apply，将看到最新注册身份。
    if (auto* Previous = OldService.Get();
        Previous && OldToken.IsValid() && (Previous != Service || OldId != NewId || OldToken != NewToken || DesiredCount == 0))
        Previous->StopAmbientFaunaForOwner(OldId, OldToken);
    if (ApplyMutationToken != Operation)
    {
        OutError = TEXT("场景群落绑定已被后续同步操作接管。");
        return false;
    }
    return true;
}

void UDivineBeastsFaunaSceneComponent::StopFauna()
{
    check(IsInGameThread());
    ApplyMutationToken = FGuid::NewGuid();
    const auto Previous = BoundPresentation;
    const FName PreviousId = BoundHabitatId;
    const FGuid PreviousToken = BoundRegistrationToken;
    BoundPresentation.Reset(); BoundHabitatId = NAME_None; BoundRegistrationToken.Invalidate();
    if (auto* Service = Previous.Get()) Service->StopAmbientFaunaForOwner(PreviousId, PreviousToken);
}


int32 UDivineBeastsFaunaSceneComponent::DisturbFauna(
    const FVector& WorldCenter, float Radius, float Strength, float DurationSeconds, FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    UWorld* World = GetWorld();
    if (bEndingPlay || !World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer)
    {
        OutError = TEXT("当前本地群落世界不可用。");
        return 0;
    }
    // 公共服务校验有限值/范围并统一更新其已有实例；组件不持有VFX服务裸指针或Niagara对象。
    if (auto* Service = IGamePlatformVFXService::Get(World))
        return Service->DisturbFauna(WorldCenter, Radius, Strength, DurationSeconds, OutError);
    OutError = TEXT("当前世界尚未注册VFX群落服务。");
    return 0;
}

void UDivineBeastsFaunaSceneComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    StopFauna();
    Super::EndPlay(Reason);
}

#if WITH_EDITOR
void UDivineBeastsFaunaSceneComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if (GetWorld() && GetWorld()->IsGameWorld() && HasBegunPlay() && !bEndingPlay)
    {
        FString Error;
        ApplyFauna(Error);
        if (!Error.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("[Fauna] 参数修改被拒绝：%s"), *Error);
    }
}
#endif
