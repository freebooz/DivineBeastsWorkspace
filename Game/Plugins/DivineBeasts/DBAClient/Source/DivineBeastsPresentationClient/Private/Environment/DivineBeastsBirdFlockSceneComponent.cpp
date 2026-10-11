// 神兽联盟客户端场景组件：Begin/EndPlay事件建立/撤销自有注册，不启动独立渲染器。
// 身份替换先验证候选再撤销旧绑定；同步完成通知可重入，必须核对本组件变更token。
#include "Environment/DivineBeastsBirdFlockSceneComponent.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

UDivineBeastsBirdFlockSceneComponent::UDivineBeastsBirdFlockSceneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

TArray<FString> UDivineBeastsBirdFlockSceneComponent::GetBirdSpeciesOptions() const
{
    return {TEXT("Seagull"), TEXT("Swallow"), TEXT("Eagle")};
}

void UDivineBeastsBirdFlockSceneComponent::BeginPlay()
{
    bEndingPlay = false;
    Super::BeginPlay();
    FString Error;
    if (!ApplyBirdFlock(Error) && !Error.IsEmpty())
        UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 场景锚点：%s"), *Error);
}

bool UDivineBeastsBirdFlockSceneComponent::ApplyBirdFlock(FString& OutError)
{
    check(IsInGameThread());
    const TStrongObjectPtr<UDivineBeastsBirdFlockSceneComponent> KeepComponent(this);
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
    const FName OldId = BoundFlockId;
    const FGuid OldToken = BoundRegistrationToken;
    const FName NewId = FlockId.IsNone() ? FName(*GetPathName()) : FlockId;
    const FGuid NewToken = OldService.Get() == Service && OldId == NewId && OldToken.IsValid()
        ? OldToken : FGuid::NewGuid();

    const int32 DesiredCount = BirdCount;
    // 不先Stop旧绑定：改ID与非法数量/物种同时发生时，失败必须留下原来的合法鸟群。
    if (!Service->StartAmbientBirdFlockForOwner(
        NewId, SpeciesId, DesiredCount, GetComponentLocation(), this, NewToken, OutError)) return false;
    if (ApplyMutationToken != Operation || bEndingPlay)
    {
        // 重入操作已接管绑定。只清理无人认领的本次候选，不能撤销内层已经接纳的同一注册。
        if (BoundPresentation.Get() != Service || BoundFlockId != NewId || BoundRegistrationToken != NewToken)
            Service->StopAmbientBirdFlockForOwner(NewId, NewToken);
        OutError = TEXT("场景鸟群配置已在同步通知中被更新或停止。");
        return false;
    }

    if (DesiredCount > 0)
    {
        BoundPresentation = Service;
        BoundFlockId = NewId;
        BoundRegistrationToken = NewToken;
    }
    else
    {
        BoundPresentation.Reset(); BoundFlockId = NAME_None; BoundRegistrationToken.Invalidate();
    }
    // 先发布新绑定再取消旧句柄；旧取消通知若重入Apply，将看到最新注册身份。
    if (auto* Previous = OldService.Get();
        Previous && OldToken.IsValid() && (Previous != Service || OldId != NewId || OldToken != NewToken || DesiredCount == 0))
        Previous->StopAmbientBirdFlockForOwner(OldId, OldToken);
    if (ApplyMutationToken != Operation)
    {
        OutError = TEXT("场景鸟群绑定已被后续同步操作接管。");
        return false;
    }
    return true;
}

void UDivineBeastsBirdFlockSceneComponent::StopBirdFlock()
{
    check(IsInGameThread());
    ApplyMutationToken = FGuid::NewGuid();
    const auto Previous = BoundPresentation;
    const FName PreviousId = BoundFlockId;
    const FGuid PreviousToken = BoundRegistrationToken;
    BoundPresentation.Reset(); BoundFlockId = NAME_None; BoundRegistrationToken.Invalidate();
    if (auto* Service = Previous.Get()) Service->StopAmbientBirdFlockForOwner(PreviousId, PreviousToken);
}

void UDivineBeastsBirdFlockSceneComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    StopBirdFlock();
    Super::EndPlay(Reason);
}

#if WITH_EDITOR
void UDivineBeastsBirdFlockSceneComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if (GetWorld() && GetWorld()->IsGameWorld() && HasBegunPlay() && !bEndingPlay)
    {
        FString Error;
        ApplyBirdFlock(Error);
        if (!Error.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 参数修改被拒绝：%s"), *Error);
    }
}
#endif
